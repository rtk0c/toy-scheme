(+ 1 2)
(+ 1 2 3 4 5 6 7 8 9)
(* 2 24)
(/ 22 7)
(- 4)
(sqrt 9)
(sqrt 341)
(if #t 1 (+ 68 1))
(if #f 1 (+ 68 1))
(null? '())
(null? '(1))
(cons 1 '(2 3))
(car '(1 2))
(cdr '(1 2))
(car (cdr '(1 2)))
(< 1 1.2)
(<= 1.2 1.2)
(> 4 12)
(>= 3 (+ 1 2))
(= 0 0)
(= "fl" "fl")
(= 'sym 'sym)

;; Test eval of builtins
define
if
car
cons
