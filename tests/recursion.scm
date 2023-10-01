(define (len lst)
  (if (null? lst)
      0
      (+ 1 (len (cdr lst)))))

(len '())
(len '(1 2 3))
(len '(1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1))

(define (remove-item lst elm)
  (if (null? lst)
      '()
      (if (= (car lst) elm)
          (remove-item (cdr lst) elm)
          (cons (car lst)
                (remove-item (cdr lst) elm)))))

(remove-item '(1 2 3 2 4) 2)
