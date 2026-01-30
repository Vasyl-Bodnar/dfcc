#! /usr/local/bin/guile -s
!#
;;; SPDX-License-Identifier: BSD-3-Clause

;; Use current folder, can also use enviroment variables here for absolute
;; You can also provide root path to (configure), but "." is default anyway
(add-to-load-path ".")
(use-modules (buildlib))

(define clean? (memq #t (map (lambda (x) (equal? "clean" x))
                             (command-line))))
(define install? (memq #t (map (lambda (x) (equal? "install" x))
                               (command-line))))
(define compile? (not clean?))

(let ((config (configure #:exe-name "dfcc" ;;#:lib-source-dir "src/lib" #:lib-name "libdfcc" #:lib-type 'both)
                         #:link '("m")
                         #:derive '(DYNAMIC_TABLE))))
  (compile-c config compile?)
  (install config install?)
  (clean config clean?))
