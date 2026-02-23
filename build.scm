#! /usr/local/bin/guile -s
!#
;; This Source Code Form is subject to the terms of the Mozilla Public
;; License, v. 2.0. If a copy of the MPL was not distributed with this
;; file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
