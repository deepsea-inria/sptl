all: sptl.html sptl.pdf

%.pdf : %.md
	pandoc $< -s -o $@

%.html : %.md
	pandoc $< -s --toc --number-sections --mathjax=http://cdn.mathjax.org/mathjax/latest/MathJax.js?config=TeX-AMS-MML_HTMLorMML -c sptl.css -o $@

clean:
	rm -f *.pdf *.html5
