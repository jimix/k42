<!--
  K42: (C) Copyright IBM Corp. 2001.
  All Rights Reserved

  This file is distributed under the GNU LGPL. You should have
  received a copy of the license along with K42; see the file LICENSE.html
  in the top-level directory for more details.

  $Id: wp.dsl,v 1.9 2001/10/15 22:26:57 jimix Exp $
-->

<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
  <!ENTITY % html "IGNORE">
  <![%html;[
    <!ENTITY % print "IGNORE">
    <!ENTITY docbook.dsl SYSTEM "html/docbook.dsl" CDATA dsssl>
  ]]>
  <!ENTITY % print "INCLUDE">
  <![%print;[
    <!ENTITY docbook.dsl SYSTEM "print/docbook.dsl" CDATA dsssl>
  ]]>
]>

<!-- Local document specific customizations. -->

<style-sheet>
<style-specification id="print" use="docbook">
<style-specification-body>

;; Abstracts should be italic
(element abstract
  (make display-group
    font-posture: 'italic
    space-before: %block-sep%
    space-after: %block-sep%
    start-indent: %body-start-indent%
    (process-children)))


;; These should be common to all output modes
(define %refentry-xref-italic%
  ;; Use italic text when cross-referencing RefEntrys?
  #t)
(define %refentry-xref-manvolnum%
  ;; Output manvolnum as part of RefEntry cross-reference?
  #t)
(define %section-autolabel%
  ;;Do you want enumerated sections? (E.g, 1.1, 1.1.1, 1.2, etc.)
  #t)

(element application ($mono-seq$))
(element envar ($mono-seq$))
(element interface ($mono-seq$))
(element interfacedefinition ($mono-seq$))
;; specific

(define tex-backend
  ;;Tex Backend on
  #t)
(define default-backend
  ;; This parameter sets the default backend.
  tex)
(define %visual-acuity%
  ;; General measure of document text size
  ;; "tiny", "normal", "presbyopic", and "large-type"
  "normal")
(define %two-side%
  ;; Is two-sided output being produced?
  #t)
(define %titlepage-n-columns%
  ;; Sets the number of columns on the title page
  1)
(define %page-n-columns%
  ;; Sets the number of columns on each page
  1)
(define %page-balance-columns?%
  ;; Balance columns on pages?
  #t)
(define %default-quadding%  
  ;; The default quadding
  'justify)

(define %left-margin%
  ;; Width of left margin
  4pi)
(define %right-margin%
  ;; Width of the right margin
  8pi)
(define %top-margin%
  ;;How big do you want the margin at the top?
  6pi)
(define %bottom-margin%
  ;; How big do you want the margin at the bottom?
  2pi)
(define %header-margin%
  ;; height of the header margin
  4pi)
(define %body-start-indent%
  ;; Default indent of body text
  4pi)
(define %show-ulinks%
  ;; Display URLs after ULinks?
  #f)
;; This does not seem to work
(define %footnote-ulinks%
  ;; Generate footnotes for ULinks?
  #f)
(define bop-footnotes
  ;; Make "bottom-of-page" footnotes?
  #t)
(define %footnote-size-factor%
  ;; Footnote font scaling factor
  0.9)
(define %show-comments%
  ;; Display Comment elements?
  #t)
(define %hyphenation%
  ;; Allow automatic hyphenation?
  #f)
(define ($object-titles-after$)
  ;; List of objects who's titles go after the object
  ;;'())
  (list (normalize "figure")))
(define %title-font-family%
  ;; What font would you like for titles?
  "Helvetica")
(define %body-font-family%
  ;; What font would you like for the body?
  "Palatino")
(define %mono-font-family%
  ;; What font would you like for mono-seq?
  "Courier New")
(define %hsize-bump-factor%
  1.1)
(define %block-sep%
  ;; Distance between block-elements
  (* %para-sep% 0.5))
(define %head-before-factor%
  ;;  Factor used to calculate space above a title
  0.01)
(define %head-after-factor%
  ;; Factor used to calculate space below a title
  0.01)

(define formal-object-float
  ;; If '#t', formal objects will float if floating is supported by the
  ;; backend. At present, only the TeX backend supports floats.
  #t)

(element phrase
  (let ((role (if (attribute-string (normalize "role"))
		  (attribute-string (normalize "role"))
		   (normalize "default"))))
    (cond
     ((equal? (normalize role) (normalize "del")) ($score-seq$ 'through))
     ((equal? (normalize role) (normalize "add")) ($score-seq$ 'after))
     (else ($charseq$)))))

;;
;; Biblio Rules
;;
(define biblio-filter-used #t)

;; this is borken if there are no XREFS
;;(define biblio-citation-check #t)

</style-specification-body>
</style-specification>

<style-specification id="html" use="docbook">
<style-specification-body>

;; These should be common to all output modes
(define %refentry-xref-italic%
  ;; Use italic text when cross-referencing RefEntrys?
  #t)
(define %refentry-xref-manvolnum%
  ;; Output manvolnum as part of RefEntry cross-reference?
  #t)
(define %section-autolabel%
  ;;Do you want enumerated sections? (E.g, 1.1, 1.1.1, 1.2, etc.)
  #t)

;; specific

;; Change depth of article TOC to include my section level 2
(define (toc-depth nd)
  (if (string=? (gi nd) (normalize "article"))
      4
      1))

(define %graphic-default-extension%
  ;;What is the default extension for images?
  "png")
(define %generate-article-toc%
  ;; Should a Table of Contents be produced for Articles?
  #t)
(define %header-navigation%
  ;; Should navigation links be added to the top of each page?
  #t)
(define %footer-navigation%
  ;; Should navigation links be added to the bottom of each page?
  #t)
(define %shade-verbatim% 
  ;; Should verbatim environments be shaded?
  #f)
(define nochunks
  ;; Suppress chunking of output pages
  #t)
(define rootchunk
  ;; Make a chunk for the root element when nochunks is used
  #f)
(define %body-attr%
  ;; What attributes should be hung off of BODY?
  (list
   (list "BGCOLOR" "#FFFFFF")
   (list "TEXT" "#000000")
   (list "LINK" "#0000FF")
   (list "VLINK" "#840084")
   (list "ALINK" "#0000FF")))
(define %html-ext%
  ;; Default extension for HTML output files
  ".html")

(define %html-pubid%
  ;; What public ID are you declaring your HTML compliant with?
  "-//W3C//DTD HTML 4.01 Transitional//EN")

(define %html-prefix%
  ;; Add the specified prefix to HTML output filenames
  "")
(define %root-filename%
  ;; Name for the root HTML document
  #f)
(define %show-comments%
  ;; Display Comment elements?
  #t)

;;
;; Biblio Rules
;;
(define biblio-filter-used #t)
;; this is borken if there are no XREFS
;;(define biblio-citation-check #t)


</style-specification-body>
</style-specification>
<external-specification id="docbook" document="docbook.dsl">
</style-sheet>

<!--
Local Variables:
mode: scheme
End:
-->
