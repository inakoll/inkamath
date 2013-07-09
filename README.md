Inkamath
========

Inkamath is an mathematical interpreter written in standard C++.
© 2013 iNaKoll. Please see accompanied license file.

Inkamath supports the definition of functions, matrices and sequences of complex numbers.
The syntax is meant to be simple, powerful and as close as possible to the common math syntax.

*Overview :*
  
  >> [pi, e]
	3.14159265 2.71828183

	>> A=[a a; a a]
	0 0
	0 0

	>> a=[1 2;3 4]
	1 2
	3 4

	>> A
	1 2 1 2
	3 4 3 4
	1 2 1 2
	3 4 3 4

	>> exp(x)_n=exp(x)_(n-1)+x^n/!n
	0

	>> exp(1)
	2.71828183

	>> cos(x)=(exp(i*x)+exp(-i*x))/2
	0
  
### Introduction ###

Inkamath est un interpreteur d'expressions mathématiques simple et ludique.
La syntaxe utilisée se veut être intuitive et au plus proche de la notation mathématique.

Pour utiliser Inkamath lancer l'exécutable, une fenêtre console s'ouvre.
Taper des expressions mathématiques après les caractères >> .
Pour évaler l'expression tapée, appuyer sur entrée.
Le résultat s'affiche alors sur la ligne en dessous votre expression mathématique.
La console affiche à nouveau les caractères >> vous invitant à taper votre
prochaine expression.

*Exemple* :
  
	>> 1+1
	2
	
	>> 3*(4+5)
	27
	
	>> a=1+2+3
	6
	
	>> a
	6
	
Inkamath n'est pas une simple calculatrice,
l'interpréteur d'expressions mathématique permet les usages les plus simples
comme les plus avancés en toute simplicité.

Les principales fonctionnalités d'Inkamath sont :

- défintion de fonctions par l'utilisateur
- calcul matriciel
- support des nombres complexes
- définition de suites ou séries par l'utilisateur
	
L'exemple ci-dessous montre comment définir la fonction exponnetielle complexe 
en une seule ligne.	

*Exemple* :
	
	>> exp(x)_n=exp(x)_(n-1)+x^n/!n
	0

	>> exp(1) 
	2.71828183
	
	>> exp(1)-e
	-2.731261528055e-008
	
Les paragraphes suivants constituent une référence rapide de la syntaxe utilisée par l'interpréteur Inkamath.

#### Quick reference ####
*Notation* :

- 'expr' 		Expression quelconque (voir définition).
- 'n' 			identifiant (voir définition).
- 'f', 'x', 'y'	référence quelconque (void définition).
- 'a' 			référence associée à une expression s'évaluant à la matrice 2x2 suivante

	1 2
	3 4

#####1. Expressions unaires#####
- +expr : plus unaire
- -expr : moins unaire
- !expr : factorielle (opérateur préfixé) 
		
#####2. Expressions binaires#####

- expr+expr : addition
- expr-expr : soutraction
- expr*expr : multiplication
- expr/expr : division
- expr^expr : puissance réelle (=exponnentielle(expr*ln(expr))
- f=expr		: assignation d'une expression à une référence (voir § 4.)
		
#####3. Matrices#####

Si 'expr' est de dimension 1x1 alors :
	[expr] 						: matrice 1x1
	[expr, expr; expr, expr] 	: matrice 2x2
	[expr; expr, expr] 			: matrice 2x2
	[expr, expr; expr, expr] 	: matrice 2x2
	
Inkamath permet l'utilisation de matrices d'expressions. 
Autrement dit, la dimension des expressions ci-dessus dépend de la dimension de l'évaluation de 'expr'.
Si 'expr' est de dimension 2x1 alors :

- [expr] 						: matrice 2x1
- [expr, expr; expr, expr] 		: matrice 4x2
- [expr; expr, expr] 			: matrice 4x2
- [expr, expr; expr, expr] 		: matrice 4x2
	
Ainsi 'a' s'évalue à :

	1 2
	3 4
	
et [a, a; a, a] s'évalue à :

	1 2 1 2
	3 4 1 2
	1 2 1 2
	3 4 1 2
			
#####4. Réferences#####
Inkamath est un langage que l'on pourrait qualifier de "fonctionnel". Les identifiants définis par l'utilsateur et que l'on pourrait au premier regard associer à des variables ou des fonctions sont en fait exclusivement des *fonctions lambda*. Les "Références" définies par l'utilsateur sont donc des identifiants associés à des expressions et non pas à des valeurs.

*Exemple :*

	>> a = 1
	1
	>> b=a+a
	2
	>> a = 2
	2
	>> b
	4

*Exemples de défintion de références utilisateur :*

	f=expr		: assignation d'une expression à une référence
	f(x)=expr	: assignation d'une expression à une référence paramétrée
	f_0=expr	: assignation d'une expression à une référence indexée
	f_n=expr	: assignation d'une expression au terme général d'une référence indexée par l'identifiant n.

On peut distinguer 3 types définitions différents d'assignations dont le sémantique est radicalement différentes.

1. Assignation simple d'une référence

C'est l'assignation usuelle qui associe l'identifiant de la référence à une expression. L'assignation simple permet de définir ce qui se rapprocherait le plus des notions variables et de fonctions dans d'autres langages de programmation.

Si l'expression 'expr' assignée à une référence 'f' ne contient que des constantes litérales (ex: expr=1+2), alors la référence est une fonction lambda ne prenant pas de paramètre. Si toutefois la référence est appelée avec des paramètres, ces derniers sont ignorés.

Exemple :
	>> f=1+2
	3

	>> f
	3

	>> f(1)
	3

Si l'expression 'expr' assignée à une référence 'f' contient (directement ou indirectement) des références utilisateurs, alors la référence 'f' définie par 'expr' est une fonction lambda prenant en paramètre des expressions à associer à chacune des références contenues dans 'expr'.

Exemple :
	>> f=x^2+y
	0

	>> f(2, 1)
	5

	>> f(x=3, y=2)
	11


2. Assignation à un index d'une référence indexée

Exemple :
	f_0=expr	: assignation d'une expression à une référence indexée

3. Assignation du terme général d'une référence indexée

Exemple :
	f_n=expr	: assignation d'une expression au terme général d'une référence indexée par l'identifiant n.

#####5. Constantes et fonctions built-in #####
Les constantes 'e' (2.71828182846) et 'i' (unité imaginaire) sont actuellement les seules définitions de constantes disponibles par défaut. La constante imaginaire pur 'i' permet le support des nombres complexes dans inkamath.

Aucune fonction built-in n'est disponbile actuellement.


Comments and contributions are welcomed.
