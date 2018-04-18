open XBase
open Params

let system = XSys.command_must_succeed_or_virtual

(*****************************************************************************)
(** Parameters *)

let arg_virtual_run = XCmd.mem_flag "virtual_run"
let arg_virtual_build = XCmd.mem_flag "virtual_build"
let arg_nb_runs = XCmd.parse_or_default_int "runs" 1
let arg_nb_seq_runs = XCmd.parse_or_default_int "seq_runs" 1
let arg_mode = Mk_runs.mode_from_command_line "mode"
let arg_skips = XCmd.parse_or_default_list_string "skip" []
let arg_onlys = XCmd.parse_or_default_list_string "only" []
let arg_benchmarks = XCmd.parse_or_default_list_string "benchmark" ["all"]
let arg_proc =
  let cmdline_proc = XCmd.parse_or_default_int "proc" 0 in
  let default =
    if cmdline_proc > 0 then
      cmdline_proc
    else
      let _ = system "get-nb-cores.sh > nb_cores" false in
      let chan = open_in "nb_cores" in
      let str = try input_line chan
      with End_of_file -> (close_in chan; "1")
      in
      int_of_string str
  in
  XCmd.parse_or_default_int "proc" default
let arg_print_err = XCmd.parse_or_default_bool "print_error" false
let arg_scheduler = XCmd.parse_or_default_string "scheduler" ""
    
let par_run_modes =
  Mk_runs.([
    Mode arg_mode;
    Virtual arg_virtual_run;
    Runs arg_nb_runs; ])

let seq_run_modes =
  Mk_runs.([
    Mode arg_mode;
    Virtual arg_virtual_run;
    Runs arg_nb_seq_runs; ])

    
(*****************************************************************************)
(** Steps *)

let select make run check plot =
   let arg_skips =
      if List.mem "run" arg_skips && not (List.mem "make" arg_skips)
         then "make"::arg_skips
         else arg_skips
      in
   Pbench.execute_from_only_skip arg_onlys arg_skips [
      "make", make;
      "run", run;
      "check", check;
      "plot", plot;
      ]

let nothing () = ()

(*****************************************************************************)
(** Files and binaries *)

let build path bs is_virtual =
   system (sprintf "make -C %s -j %s" path (String.concat " " bs)) is_virtual

let file_results exp_name =
  Printf.sprintf "results_%s.txt" exp_name

let file_tables_src exp_name =
  Printf.sprintf "tables_%s.tex" exp_name

let file_tables exp_name =
  Printf.sprintf "tables_%s.pdf" exp_name

let file_plots exp_name =
  Printf.sprintf "plots_%s.pdf" exp_name

(** Evaluation functions *)

let eval_exectime = fun env all_results results ->
  Results.get_mean_of "exectime" results

let eval_exectime_stddev = fun env all_results results ->
  Results.get_stddev_of "exectime" results

let string_of_millions ?(munit=false) v =
   let x = v /. 1000000. in
   let f = 
     if x >= 10. then sprintf "%.0f" x
     else if x >= 1. then sprintf "%.1f" x
     else if x >= 0.1 then sprintf "%.2f" x
     else sprintf "%.3f" x in
   f ^ (if munit then "m" else "")
                        
let formatter_settings = Env.(
    ["prog", Format_custom (fun s -> "")]
  @ ["library", Format_custom (fun s -> s)]
  @ ["n", Format_custom (fun s -> sprintf "Input: %s million 32-bit ints" (string_of_millions (float_of_string s)))]
  @ ["proc", Format_custom (fun s -> sprintf "#CPUs %s" s)]
  @ ["promotion_threshold", Format_custom (fun s -> sprintf "F=%s" s)]
  @ ["threshold", Format_custom (fun s -> sprintf "K=%s" s)]
  @ ["block_size", Format_custom (fun s -> sprintf "B=%s" s)]      
  @ ["operation", Format_custom (fun s -> s)])

let default_formatter =
  Env.format formatter_settings
    
let string_of_percentage_value v =
  let x = 100. *. v in
  (* let sx = if abs_float x < 10. then (sprintf "%.1f" x) else (sprintf "%.0f" x)  in *)
  let sx = sprintf "%.1f" x in
  sx
    
let string_of_percentage ?(show_plus=true) v =
   match classify_float v with
   | FP_subnormal | FP_zero | FP_normal ->
       sprintf "%s%s%s"  (if v > 0. && show_plus then "+" else "") (string_of_percentage_value v) "\\%"
   | FP_infinite -> "$+\\infty$"
   | FP_nan -> "na"

let string_of_percentage_change ?(show_plus=true) vold vnew =
  string_of_percentage ~show_plus:show_plus (vnew /. vold -. 1.0)

let rec generate_in_range_by_incr first last incr =
  if first > last then
    [last]
  else
    first :: (generate_in_range_by_incr (first +. incr) last incr)

(*****************************************************************************)
(** A benchmark to find a good setting for kappa *)

module ExpFindKappa = struct

let name = "find-kappa"

let prog = "spawnbench.sptl"

let prog_elision = "spawnbench.sptl_elision"

let kappas =
  generate_in_range_by_incr 0.2 40.0 2.0

let mk_kappa =
  mk float "sptl_kappa"
    
let mk_kappas = fun e ->
  let f kappa =
    Env.add Env.empty mk (float kappa)
  in
  List.map f kappas
    
let mk_alpha =
  mk float "sptl_alpha" 1.3

let mk_custom_kappa =
  mk int "sptl_custom_kappa" 1

let mk_proc =
  mk int "proc" 1

let mk_configs =
  mk_custom_kappa & (mk_list float "sptl_kappa" kappas) & mk_alpha & mk_proc

let make() =
  build "." [prog; prog_elision;] arg_virtual_build

let nb_runs = 10
    
let run_modes =
  Mk_runs.([
    Mode arg_mode;
    Virtual arg_virtual_run;
    Runs nb_runs; ])
    
let run() = (
  Mk_runs.(call (run_modes @ [
    Output (file_results prog);
    Timeout 400;
    Args (
      mk_prog prog
    & mk_configs)]));
  Mk_runs.(call (run_modes @ [
    Output (file_results prog_elision);
    Timeout 400;
    Args (
      mk_prog prog_elision)])))

let check = nothing  (* do something here *)

let spawnbench_formatter =
 Env.format (Env.(
   [ ("n", Format_custom (fun n -> sprintf "spawnbench(%s)" n)); ]
  ))

let plot() =
  let results_all = Results.from_file (file_results prog) in
  let env = Env.empty in
  let kappa_exectime_pairs = ~~ List.map kappas (fun kappa ->
    let [col] = ((mk_prog prog) & mk_custom_kappa & (mk_kappa kappa) & mk_alpha) env in
    let results = Results.filter col results_all in
    let e = Results.get_mean_of "exectime" results in
    (kappa, e))
  in
  let elision_exectime =
    let results_all = Results.from_file (file_results prog_elision) in
    let [col] = (mk_prog prog_elision) env in
    let results = Results.filter col results_all in
    Results.get_mean_of "exectime" results
  in
  let rec find kes =
    match kes with
      [] ->
        let (kappa, _) = List.hd (List.rev kappa_exectime_pairs) in
        kappa
    | (kappa, exectime) :: kes ->
        if exectime < elision_exectime +. 0.05 *. elision_exectime then
          kappa
        else
          find kes
  in
  let kappa = find kappa_exectime_pairs in
  let oc = open_out "kappa" in
  let _ = Printf.fprintf oc "%f\n" kappa in
  close_out oc

let all () = select make run check plot

end

(*****************************************************************************)
(** A benchmark to find a good setting for alpha *)

module ExpFindAlpha = struct

let name = "find-alpha"

let prog = "spawnbench.sptl"

let alphas =
  generate_in_range_by_incr 1.0 3.0 0.4
    
let mk_alphas =
  mk_list float "sptl_alpha" alphas

let arg_kappa =
  if Sys.file_exists "kappa" then
   let ic = open_in "kappa" in
   try
     let line = input_line ic in
     close_in ic;
     float_of_string line
   with e ->
     close_in_noerr ic;
     20.0
  else
    20.0

let mk_kappa =
  mk float "sptl_kappa" arg_kappa

let mk_custom_kappa =
  ExpFindKappa.mk_custom_kappa

let mk_proc =
  mk int "proc" arg_proc
    
let mk_configs =
  mk_custom_kappa & mk_kappa & mk_alphas & mk_proc & (mk int "n" 1000000000)

let make() =
  build "." [prog] arg_virtual_build

let nb_runs = 10
    
let run_modes =
  Mk_runs.([
    Mode arg_mode;
    Virtual arg_virtual_run;
    Runs nb_runs; ])
    
let run() =
  Mk_runs.(call (ExpFindKappa.run_modes @ [
    Output (file_results name);
    Timeout 400;
    Args (
      mk_prog prog
    & mk_configs)]))

let check = nothing  (* do something here *)

let spawnbench_formatter =
 Env.format (Env.(
   [ ("n", Format_custom (fun n -> sprintf "spawnbench(%s)" n)); ]
  ))

let plot() =
  let results_all = Results.from_file (file_results name) in
  let env = Env.empty in
  let alpha_exectime_pairs = ~~ List.map alphas (fun alpha ->
    let [col] = ((mk_prog prog) & (mk float "sptl_alpha" alpha)) env in
    let results = Results.filter col results_all in
    let e = Results.get_mean_of "exectime" results in
    (alpha, e))
  in
  let rec find aes (min_alpha, min_exectime) =
    match aes with
      [] ->
        min_alpha
    | (alpha, exectime) :: aes ->
        if exectime < min_exectime then
          find aes (alpha, exectime)
        else
          find aes (min_alpha, min_exectime)
  in
  let alpha = find alpha_exectime_pairs (1.3, max_float) in
  let oc = open_out "alpha" in
  let _ = Printf.fprintf oc "%f\n" alpha in
  close_out oc

let all () = select make run check plot

end
   
(*****************************************************************************)
(** Main *)

let _ =
  let arg_actions = XCmd.get_others() in
  let bindings = [
    "find-kappa",                  ExpFindKappa.all;
    "find-alpha",                  ExpFindAlpha.all;    
  ]
  in
  system "mkdir -p _results" false;
  Pbench.execute_from_only_skip arg_actions [] bindings;
  ()
