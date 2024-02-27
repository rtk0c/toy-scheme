;; => '()
(define a 1)
;; => 1
a

;; => '()
(define a 2)
;; => 2
a

;; => '()
(define my-list '(1 2 3 "hello"))
;; => (1 2 3 "hello")
my-list

;; => '()
(define b 40)
;; => 42
(+ a b)

;; => '()
(define (my-add a b)
  (+ a b))
;; => 3
(my-add 1 2)

;; Quadratic formula calculation
;; tests nested user-proc calling, and more complex body forms

;; => '()
(define (sq x) (* x x))
;; => '()
(define (calc a b c)
  (/ (+ (- b)
        (sqrt (- (sq b)
                 (* 4 a c))))
     (* 2 a)))
;; => 1
(calc 1 0 -1)
;; => -0.46572697
(calc -12 3 4)

;; => #PROC:my-add
my-add
;; => #PROC:calc
calc
