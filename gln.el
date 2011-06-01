;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Copyright (c) 2011 Scott Vokes <vokes.s@gmail.com>
;; 
;; This file is not part of Emacs. In fact, it's ISC licensed.
;;
;; Permission to use, copy, modify, and/or distribute this software for
;; any purpose with or without fee is hereby granted, provided that the
;; above copyright notice and this permission notice appear in all
;; copies.
;;
;; THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
;; WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
;; WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
;; AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
;; DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
;; PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
;; TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
;; PERFORMANCE OF THIS SOFTWARE.
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Simple Emacs wrapper for glean.
;;
;; Usage:
;;
;;    Build index with M-x glean-index .
;;
;;    Search with M-x glean .
;;
;; In the results window, pressing enter on a filename will find it, 
;; and 'q' buries the buffer.
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defvar glean-db-path (getenv "HOME")
  "Path to glean databases.")

(defvar glean-index-root (getenv "HOME")
  "Root path for files to index.")

(defun glean (query &optional db-path)
  "Search with glean."
  (interactive "sGlean search: ")
  (let ((buf (get-buffer "*glean-results*"))
        (db (or db-path glean-db-path)))
    (when buf (kill-buffer buf))
    (let* ((query-words (split-string query))
           (args `("gln" "*glean-results*"
                         "gln" "-d" ,db
                         ,@query-words))
           (proc (apply #'start-process args))
           (buf (get-buffer "*glean-results*")))
      (with-current-buffer buf
        ;; should probably create a mode keymap...
        (local-set-key (kbd "RET") 'ffap)
        (local-set-key (kbd "q") 'delete-window)
        (toggle-read-only 1))
      (switch-to-buffer-other-window buf))))

(defun glean-index ()
  "Build a glean index."
  (interactive)
  (start-process "gln_index" "*glean-index*"
                 "gln_index" "-p" "-d" glean-db-path "-r" glean-index-root)
  (let ((buf (get-buffer "*glean-index*")))
    (when buf (switch-to-buffer-other-window buf))))

(defvar glean-font-lock-keywords
  '(("^[^:]*" . 'font-lock-bold-face))
  "Font lock highlighting for glean-results-mode.")

(define-derived-mode glean-results-mode fundamental-mode "gln"
  "Major mode for viewing glean results."
  (set (make-local-variable 'font-lock-defaults) '(glean-font-lock-keywords)))
