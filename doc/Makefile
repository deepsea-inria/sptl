all: sptl.html sptl.pdf

%.pdf : %.md
	pandoc $< -s -o $@

%.html : %.md
	pandoc $< -s --toc --number-sections --mathjax -c sptl.css -o $@

clean:
	rm -f *.pdf *.html
