;; K42: (C) Copyright IBM Corp. 2000.
;; All Rights Reserved
;;
;; This file is distributed under the GNU LGPL. You should have
;; received a copy of the license along with K42; see the file LICENSE.html
;; in the top-level directory for more details.
;;
;; $Id: kitchawan.el,v 1.10 2000/05/11 11:30:17 rosnbrg Exp $


;; Module Description:  SETUP EMACS FOR K42 INFO FILES

;; 
;; ----------------------------------

;; TODO - PUT IN HERE TO FUTURE K42 INFO FILES


;;; PS-PRINT
;; use "lpr" instead of "lp" on all the platforms we care about
;(setq ps-lpr-command "lpr")

;; [BG] Some helpful conveniences

;(autoload 'bc     "bc" "bc-mode (radix 16)" t)
;(autoload 'bc-dec "bc" "bc-mode (radix 10)" t)

(autoload 'tag-complete-symbol "etags")
;(define-key global-map "\C-\\"   'tag-complete-symbol)
;(setq tag-table-alist
;      '(
;	 ("" . "../")
;	 ("" . "../../")
;	 ("" . "../../../")
;	 ("" . (concat (getenv "KITCHTOP") "include/"))
;	 ("" . (concat (getenv "KITCHTOP") "xobj/"))
;	 ("" . (concat (getenv "KITCHTOP") "obj/"))
;	 ))


	     
(defun kitch-get-mkanchor ()
   (save-excursion
     (let (mkanchor end
	  (buffer (get-buffer-create "*find-kitchawan-mkanchor*")))
	  (set-buffer buffer)
	  (erase-buffer)
	  (call-process "bash" nil t nil "-c" "kanchor -echo")
	  (if (= (point-min) (point-max))
	      (setq mkanchor (getenv "MKANCHOR"))
	    (goto-char (point-min))
	    (setq end (if (search-forward "\n" nil t) (point) (point-max)))
	    (setq mkanchor (buffer-substring (point-min) (- end 1))))
	  (kill-buffer buffer)
	  mkanchor
	  )))

(defun kitch-add-make-header ()
  "add the standard k42 header for Makefiles"
  (interactive)
  (insert-file-contents
   (concat (kitch-get-mkanchor) "install/etc/stdhdr.make"))
  )

(defun kitch-add-asm-header ()
  "add the standard k42 header for ASM"
  (interactive)
  (insert-file-contents
   (concat (kitch-get-mkanchor) "install/etc/stdhdr.c"))
  )

(defun kitch-add-C-header ()
    "add the standard k42 header for C"
    (interactive)
    (insert-file-contents
     (concat (kitch-get-mkanchor) "install/etc/stdhdr.c"))
    )

(defun kitch-add-html-header ()
    "add the standard k42 header for html"
    (interactive)
    (insert-file-contents
     (concat (kitch-get-mkanchor) "install/etc/stdhdr.html"))
    )

(add-hook 'asm-mode-hook 
	  '(lambda()  (define-key asm-mode-map "\C-j" 'newline)
	     ))


(add-hook 'c-mode-common-hook 
	  '(lambda() 
	     (define-key c-mode-map "\C-m" 'reindent-then-newline-and-indent)
	     (define-key c-mode-map "\C-j" 'newline)

	     (c-set-style "bsd")
	     (setq comment-column 40)
	     ))



;; SETTING UP EMACS FOR K42 DEBUGGING
;; --------------------------------------

(defun ppc-gdb (path)
  (interactive "fRun psim version gdb on file: ")
	 
  (let (gdb-command-name)
    (setq gdb-command-name "/k42/psimdbg/bin/powerpc-unknown-eabi-gdb")
    (gdb (if (string-match "XEmacs\\|Lucid" emacs-version)
	     path
	   (concat gdb-command-name " " path))
	 )
    ))
