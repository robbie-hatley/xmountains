/*
 * Recursive update procedure for fractal landscapes
 *
 * The only procedures needed outside this file are
 *   make_fold, called once to initialise the data structs.
 *   next_strip, each call returns a new strip off the side of the surface
 *                you can keep calling this as often as you want.
 *   free_strip, get rid of the strip when finished with it.
 *   free_fold,  get rid of the data structs when finished with this surface.
 *
 * Apart from make_fold all these routines get their parameters from their
 * local Fold struct, make_fold initialises all these values and has to
 * get it right for the fractal to work. If you want to change the fractal
 * dim in mid run you will have to change values at every level.
 * each recursive level only calls the level below once for every two times it
 * is called itself so it will take a number of iterations for any changes to
 * be notices by the bottom (long length scale) level.
 * The surface always starts as perturbations from a flat surface at the
 * mean value passed as a parameter to make_fold. It will therefore take
 * a number of iterations for long length scale deformations to build up.
 */
#include <math.h>
#include "crinkle.h"

char calcalt_Id[] = "$Id: calcalt.c,v 1.2 1993/02/19 12:12:20 spb Exp $";

/*{{{  Strip *make_strip(int level) */
Strip *make_strip(int level)
{
  Strip *p;
  int i , points;

  p = (Strip *) malloc( sizeof(Strip) );
  if( p == NULL )
  {
    printf("malloc failed\n");
    exit(1);
  }
  p->level = level;
  points = (1 << level) +1;
  p->d = (Height *)malloc( points * sizeof(Height) );
  if( p->d == NULL )
  {
    printf("malloc failed\n");
    exit(1);
  }
  return(p);
}
/*}}}*/
/*{{{  void free_strip(Strip *p) */
void free_strip(Strip *p)
{
  free(p->d);
  free(p);
}
/*}}}*/
/*{{{  Strip *double_strip(Strip s) */
Strip *double_strip(Strip *s)
{
  Strip *p;
  Height *a, *b;
  int i;

  p = make_strip((s->level)+1);
  a = s->d;
  b = p->d;
  for(i=0; i < (1<<s->level); i++)
  {
    *b = *a;
    a++;
    b++;
    *b = 0;
    b++;
  }
  *b = *a;
  return(p);
}
/*}}}*/
/*{{{  Strip *set_strip(int level, Height value) */
Strip *set_strip(int level, Height value)
{
  int i;
  Strip *s;
  Height *h;

  s = make_strip(level);
  h = s->d;
  for( i=0 ; i < ((1<<level)+1) ; i++)
  {
    *h = value;
    h++;
  }
  return(s);
}
/*}}}*/
/*{{{  void side_update(Strip *strip, Length scale) */
/* fill in the blanks in a strip that has just been doubled
 * this could be combined with the double routine but it would
 * make the code even messier than it already is
 */
void side_update(Strip *strip, Length scale)
{
  int count;
  int i;
  Height *p;

  count = ( 1 << (strip->level - 1));
  p = strip->d;
  for(i=0 ; i<count ; i++)
  {
    *(p+1) = ((scale * gaussian()) + (*p + *(p+2))/2.0) ;
    p += 2;
  }
}
/*}}}*/
/*{{{  void mid_update(Strip *left, Strip *new, Strip *right,Length scale, Length midscale) */
/* Calculate a new strip using the two strips to either side.
 * the "left" strip should be only half the size of the other
 * two.
 */
void mid_update(Strip *left, Strip *new, Strip *right,Length scale, Length midscale)
{
  int count;
  int i;
  Height *l , *n , *r;

  if( (left->level != (new->level - 1)) || (new->level != right->level))
  {
    printf("mid_update: inconsistant sizes\n");
    exit(2);
  }
  count = ( 1 << left->level);
  l = left->d;
  n = new->d;
  r = right->d;
  for(i=0 ; i<count ; i++)
  {
    *n = ((scale * gaussian()) + (*l + *r)/2.0) ;
    n++;
    *n = ((midscale * gaussian()) + (*l + *(l+1) + *r + *(r+2))/4.0) ;
    n++;
    l++;
    r += 2;
  }
  /* the last one */
  *n = ((scale * gaussian()) + (*l + *r)/2.0) ;
}
/*}}}*/
/*{{{  void recalc(Strip *left, Strip *regen, Strip *right,Length scale) */
/* Recalculate all the old values using the points we have just 
 * generated. This is a little idea of mine to get rid of the
 * creases. However it may change the effective fractal dimension
 * a litle bit. But who cares ?
 */
void recalc(Strip *left, Strip *regen, Strip *right,Length scale)
{
  int count;
  int i;
  Height *l , *g , *r;

  if((left->level != regen->level) || (regen->level != right->level))
  {
    printf("mid_update: inconsistant sizes\n");
    exit(2);
  }
  count = ( (1 << (regen->level-1)) - 1 );
  l = left->d;
  g = regen->d;
  r = right->d;
  *g = ((scale * gaussian()) + (*l + *(g+1) + *r)/3.0) ;
  g += 2;
  l += 2;
  r += 2;
  for(i=0 ; i<count ; i++)
  {
    *g = ((scale * gaussian()) + (*l + *(g+1) + *(g-1) + *r)/4.0) ;
    g += 2;
    l += 2;
    r += 2;
  }
  /* the last one */
  *g = ((scale * gaussian()) + (*l + *(g-1) + *r)/3.0) ;
}
/*}}}*/
/*{{{  Strip *next_strip(Fold *fold) */
Strip *next_strip(Fold *fold)
{
  Strip *result;

  if( fold->level == 0)
  {
    /*{{{  generate values from scratch */
    result=make_strip(0);
    result->d[0] = fold->mean + (fold->scale * gaussian());
    result->d[1] = fold->mean + (fold->scale * gaussian());
    return(result);
    /*}}}*/
  }
  switch(fold->state)
  {
    case START:
      /*{{{  perform an update. return first result */
      /*
       * new is NULL
       * working is NULL
       * regen is a partial strip, only odd values are valid,
       * old is a fully calculated strip
       */
      fold->new = next_strip(fold->next);
      side_update(fold->regen,fold->scale);
      fold->working = make_strip(fold->level);
      mid_update(fold->new,fold->working,fold->regen,
                  fold->scale,fold->midscale);
      if( fold->smooth )
      {
        recalc(fold->working,fold->regen,fold->old,fold->scale);
      }
      result = fold->old;
      fold->state = STORE;
      return(result);
      /*}}}*/
    case STORE:
      /*{{{  return second value from previous update. */
      result = fold->regen;
      fold->old = fold->working;
      fold->working = NULL;
      fold->regen = double_strip(fold->new);
      free_strip(fold->new);
      fold->new = NULL;
      fold->state = START;
      return(result);
      /*}}}*/
    default:
      printf("next_strip: invalid state\n");
      exit(3);
  }
  return(NULL);
}
/*}}}*/
/*{{{  Fold *make_fold(int levels, int smooth, Length len, Height start, Height mean, float fdim) */
/*
 * Initialise the fold structures.
 * As everything else reads the parameters from their fold
 * structs we need to set these here,
 * levels is the number of levels of recursion below this one.
 *    Number of points = 2^levels+1
 * smooth turns the smoothing algorithm on or off
 * len is the length of the side of the square at this level.
 *   N.B this means the update square NOT the width of the fractal.
 *   len gets smaller as the level increases.
 * mean is the mean height.
 * fdim is the fractal dimension
 */
Fold *make_fold(int levels, int smooth, Length length, Height start, Height mean, float fdim)
{
  Fold *p;
  Length scale, midscale;
  double root2;

  p = (Fold *)malloc(sizeof(Fold));
  if( p == NULL )
  {
    printf("malloc failed\n");
    exit(1);
  }
  root2=sqrt((double) 2.0 );
  scale = pow((double) length, (double) (2.0 * fdim));
  midscale = pow((((double) length)*root2), (double) (2.0 * fdim));
  p->level = levels;
  p->state = 0;
  p->smooth = smooth;
  p->mean = mean;
  p->scale = scale;
  p->midscale = midscale;
  p->new = NULL;
  p->working = NULL;
  if( levels != 0 )
  {
    p->regen = set_strip(levels,start);
    p->old = set_strip(levels,start);
    p->next = make_fold((levels-1),smooth,(2.0*length),start,mean,fdim);
  }else{
    p->regen = NULL;
    p->old = NULL;
    p->next = NULL;
  }
  return( p );
}
/*}}}*/
/*{{{  void free_fold(Fold *f) */
void free_fold(Fold *f)
{
  if( f->new != NULL )
  {
    free_strip(f->new);
  }
  if( f->working != NULL )
  {
    free_strip(f->working);
  }
  if( f->regen != NULL )
  {
    free_strip(f->regen);
  }
  if( f->old != NULL )
  {
    free_strip(f->old);
  }
  if( f->next != NULL )
  {
     free_fold(f->next);
  }
  free(f);
  return;
}
/*}}}*/
