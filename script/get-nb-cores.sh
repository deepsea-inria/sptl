#!/bin/sh

nb_cores=$( hwloc-ls --only core | wc -l )

echo $nb_cores
