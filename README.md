Inkamath
========

Inkamath is an mathematical interpreter written in standard C++.
© 2013 iNaKoll. Please see accompanied license file.

Inkamath supports the definition of functions, matrices and sequences of complex numbers.
The syntax is meant to be simple, powerful and as close as possible to the common math syntax.
Comments and contributions are welcomed.

*Quick Overview :*
```
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
```
  
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
```
>> 1+1
2

>> 3*(4+5)
27

>> a=1+2+3
6

>> a
6
```

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
```
>> exp(x)_n=exp(x)_(n-1)+x^n/!n
0

>> exp(1) 
2.71828183

>> exp(1)-e
-2.731261528055e-008
```
	
Les paragraphes suivants constituent une référence rapide de la syntaxe utilisée par l'interpréteur Inkamath.

#### Quick reference ####
*Notation* :

- 'expr' 		Expression quelconque (voir définition).
- 'n' 			identifiant (voir définition).
- 'f', 'x', 'y'	référence quelconque (void définition).
- 'a' 			référence associée à une expression s'évaluant à la matrice 2x2 suivante

```
1 2
3 4
```

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
```
[expr] 				: matrice 1x1
[expr, expr; expr, expr] 	: matrice 2x2
[expr; expr, expr] 		: matrice 2x2
[expr, expr; expr, expr] 	: matrice 2x2
```
	
Inkamath permet l'utilisation de matrices d'expressions. 
Autrement dit, la dimension des expressions ci-dessus dépend de la dimension de l'évaluation de 'expr'.
Si 'expr' est de dimension 2x1 alors :
```
[expr] 				: matrice 2x1
[expr, expr; expr, expr] 	: matrice 4x2
[expr; expr, expr] 		: matrice 4x2
[expr, expr; expr, expr] 	: matrice 4x2
```
	
Ainsi 'a' s'évalue à :

```
1 2
3 4
```
	
et [a, a; a, a] s'évalue à :
```
1 2 1 2
3 4 1 2
1 2 1 2
3 4 1 2
```
			
#####4. Réferences#####
Inkamath est un langage que l'on pourrait qualifier de "fonctionnel". Les identifiants définis par l'utilsateur et que l'on pourrait au premier regard associer à des variables ou des fonctions sont en fait exclusivement des *fonctions lambda*. Les "Références" définies par l'utilsateur sont donc des identifiants associés à des expressions et non pas à des valeurs.

*Exemple :*
```
>> a = 1
1
>> b=a+a
2
>> a = 2
2
>> b
4
```

*Exemples de défintion de références utilisateur :*
```
f=expr		: assignation d'une expression à une référence
f(x)=expr	: assignation d'une expression à une référence paramétrée
f_0=expr	: assignation d'une expression à une référence indexée
f_n=expr	: assignation d'une expression au terme général d'une référence indexée par l'identifiant n.
```
	
L'assignation étant une expression comme une autre, on peut trouver une assignation aussi bien dans la liste des paramètres d'une référence ou dans la partie droite d'une autre assignation. La portée des références suivant l'emplacement de leur assignation est défini au paragraphe 6.

On peut distinguer trois types définitions différents d'assignations dont le sémantique est radicalement différentes et qui permettent de créer trois types de références différents. Ces trois types d'assignation sont définis aux paragraphes 4.1, 4.2 et 4.3. 

######4.1. Assignation d'une référence simple######

C'est l'assignation usuelle qui associe l'identifiant de la référence à une expression. L'assignation simple permet de définir ce qui se rapprocherait le plus des notions variables et de fonctions dans d'autres langages de programmation.

Si l'expression 'expr' assignée à une référence 'f' ne contient que des constantes litérales (ex: expr=1+2), alors la référence est une fonction lambda ne prenant pas de paramètre. Si toutefois la référence est appelée avec des paramètres, ces derniers sont ignorés.

*Exemple :*
```
>> f=1+2
3

>> f
3

>> f(1)
3
```

Si l'expression 'expr' assignée à une référence 'f' contient (directement ou indirectement) des références utilisateurs, alors la référence 'f' définie par 'expr' est une fonction lambda prenant en paramètre des expressions à associer à chacune des références contenues dans 'expr'.

*Exemple :*
```
>> f=x^2+y
0

>> f(2, 1)
5

>> f(x=3, y=2)
11
```

######4.2. Assignation d'une référence indexée######

Les références indexées permettent de définir des suites et des séries au sens mathématique. Une référence indexée associe à une valeur entière une expression qui la définie. Une référence indexée peut ainsi être associée à plusieurs expressions.

*Exemple :*
```
>> f_0=1+2
3

>> f_1=3+4
7

>> f_0
3

>> f_1
7
```

Dans l'exemple ci-dessus, l'expression '1+2' est associée à l'indice 0 de la référence 'f' et l'expression '3+4' est associée à l'indice 1 de la référence 'f'.

Il est important de noter que l'assignation à un indice ne peut se faire qu'à une expression constante et entière. 

*Exemple :*
```
>> f_(0.5)=1
```

L'exemple ci-dessus est mal-formé car 0.5 n'est pas une valeur entière.

Les références indexée permettent de définir des valeurs singulières de suites mathématiques comme les valeurs initiales de suites définies par récurrence.


######4.3. Assignation du terme général d'une référence indexée######

Une suite mathématique, définie par récurence ou non, peut être définie par un terme général.
Le terme général d'une référence indexée est analogue au terme général d'une suite mathématique. Celui-ci permet d'associer à tout nombre entier une expression permettant de définir une référence indexée.

*Exemple :*
```
>> f_n=2*n
0

>> f_1
2

>> f_2
4
```

L'exemple ci-dessous assigne une expression '2*n' au terme général d'une référence 'f' indexée par l'identifiant 'n'.
	
Si l'identifiant (ici 'n') utilisé pour la définition du terme général d'une référence a été préalablement associé à une autre référence, cet identifiant perd la sémantique de la référence dans le contexte de la définition de la référence.

*Exemple :*
```
>> x=1
1

>> f_x=2*x
0

>> f_1
2

>>f_2
4
```

Une référence indexée peut n'avoir qu'un seul terme général et une infinité de définition singulière.

*Exemple :*
```
>> f_0=2
2

>> f_1=3
3

>> f_n=2*n
0

>> f_0
2

>> f_1
3

>> f_2
4

>> f_3
6
```

Dans l'exemple ci-dessus, des expressions singulières sont définies pour 'f_0' et 'f_1' et un terme général 2*n est associé à la référence indexée 'f_n'.

Une définition singulière est toujours préférée au terme général d'une référence indexée lors de l'évaluation et ce peu importe l'ordre de définition.

#####5. Constantes et fonctions built-in #####
Les constantes 'e' (2.71828182846) et 'i' (unité imaginaire) sont actuellement les seules définitions de constantes disponibles par défaut. La constante imaginaire pur 'i' permet le support des nombres complexes dans inkamath.

Aucune fonction built-in n'est disponbile actuellement.

#####6. Portée des références #####

Lorsqu'une référence est définie au plus au niveau, celle-ci est accessible dans toutes les expressions suivantes. Si l'interpréteur rencontre un identifiant non associé à une référence, l'identifiant est évalué à la valeur nulle et aucune erreur n'est remontée de la part de l'interpréteur. Les éventuels paramètres et index autour de cet identifiant sont ignorés.

*Exemple :*
```
>> x			
0			: 0 car 'x' n'est pas associé à une référence

>> x(2)_3
0			: 0 car 'x' n'est pas associé à une référence

>> f(x)=x^2
0			: 0 car 'x' n'est pas associé à une référence

>> f(2)
4			: 4 car l'expression '2' est associée à 'x' le temps de l'éluation de 'f'

>> x
0			: 0 car 'x' n'est pas associé à une référence

>> f
0			: 0 car 'x' n'est pas associé à une référence

>> f(x=4)
16			: 16 car l'expression '4' est associée à 'x' le temps de l'éluation de 'f'

>> x
0			: 0 car 'x' n'est pas associé à une référence

>> x=5
5			: 5 car l'expression '5' est associée à 'x' au plus au niveau

>> f			: 25 car l'expression '5' associée à 'x' est visible depuis 'f'
25
```

Lorsque des paramètres sont passés à une référence, l'utilisateur a la possibilité de nommer explicitement les paramètres ou non. Lorsque les paramètres ne sont pas nommé, l'ordre des paramètres de la définition de la référence est utilisé pour assigner les expressions des paramètres.

Lorsqu'une référence définie explicitement avec des paramètres est appelée sans paramètre ou lorsque le nombre de paramètre d'appel est inférieur au nombre des paramètres explicites, les paramètres d'appels non initialisé prennent leur valeur par défaut par ordre de priorité comme suit :
	- l'expression par défaut spécifié pour le paramètre lors de la définition de la référence
	- l'expression associée à la première référence visible portant le nom du paramètre
	- l'expression '0'



