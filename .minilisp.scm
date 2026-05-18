;; .minilisp.scm - пользовательские настройки и утилиты

;; Удобные сокращения
(define caar (lambda (x) (car (car x))))
(define cadr (lambda (x) (car (cdr x))))
(define cdar (lambda (x) (cdr (car x))))
(define cddr (lambda (x) (cdr (cdr x))))

;; Полезные функции
(define not (lambda (x) (if x #f #t)))

;; Арифметические предикаты
(define > (lambda (a b) (< b a)))
(define >= (lambda (a b) (or (> a b) (= a b))))
(define <= (lambda (a b) (or (< a b) (= a b))))

;; Итераторы
(define map (lambda (f lst)
  (if (null? lst)
      ()
      (cons (f (car lst)) (map f (cdr lst))))))

(define filter (lambda (pred lst)
  (if (null? lst)
      ()
      (if (pred (car lst))
          (cons (car lst) (filter pred (cdr lst)))
          (filter pred (cdr lst))))))

(define foldl (lambda (f init lst)
  (if (null? lst)
      init
      (foldl f (f init (car lst)) (cdr lst)))))

(define foldr (lambda (f init lst)
  (if (null? lst)
      init
      (f (car lst) (foldr f init (cdr lst))))))

(define range (lambda (n)
  (let loop ((i 0) (acc ()))
    (if (< i n)
        (loop (+ i 1) (cons i acc))
        (reverse acc)))))

;; Приветствие
(display "MiniLisp initialized!")
(newline)
