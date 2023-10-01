(define a 1)
a
(define a 2)
a
(define my-list '(1 2 3 "hello"))
my-list
(define b 40)
(+ a b)

(define (my-add a b)
  (+ a b))
(my-add 1 2)

;; Quadratic formula calculation
;; tests nested user-proc calling, and more complex body forms

(define (sq x) (* x x))
(define (calc a b c)
  (/ (+ (- b)
        (sqrt (- (sq b)
                 (* 4 a c))))
     (* 2 a)))
(calc 1 0 0)
(calc -12 3 4)
