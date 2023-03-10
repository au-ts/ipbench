#!/bin/sh
# histogram --- create an ASCII histogram of a column of the input
# Copyright (C) 2000-2001  Riku Saikkonen
#
# Author: Riku Saikkonen <Riku.Saikkonen@hut.fi>
# Version: 1.0 (October 28th, 2001)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# Requires: a sh-compatible shell and GNU Awk
#
# Syntax: histogram [-c] [COLUMN [MIN [MAX [STEP [PREC]]]]]
#   If MIN, MAX, STEP and PREC are specified, count from MIN to MAX in
#   steps of STEP, showing PREC digits after the decimal point.
#   -c = also print cumulative sums
#
# Defaults: COLUMN = 1 (first column)
#           only show the values that are listed (no MIN, MAX, STEP, PREC)
#           MAX = 6, STEP = 0.5, PREC = 1
#
# Examples: histogram <datafile
#           histogram 2 <datafile
#           histogram 5 0 <datafile
#           histogram 4 0 5 0.1 1 <datafile

# Change log:
# $Id$
#
# Version 1.0 (October 28th, 2001)
#  * Initial public release.

column=1
min=0
max=6
step=0.5
prec=1
hasstep=0
cumulative=0

if [ "$1" = "-c" ]; then
    cumulative=1
    shift
fi

[ "$1" != "" ] && column="$1"
[ "$2" != "" ] && min="$2"
[ "$3" != "" ] && max="$3"
[ "$4" != "" ] && step="$4"
[ "$5" != "" ] && prec="$5"
[ "$2" != "" ] && hasstep=1

if [ "$cumulative" = 1 -a "$hasstep" = 0 ]; then
    echo "Error: Using -c without MIN,MAX,STEP not supported!" 1>&2
    exit 1
fi

gawk '
function printline(key, value, numerickey,     j) {
      csum += value;
      if (numerickey)
        printf("%s\t", key);
      else
        printf("%.*f\t", ('"$prec"'), key);
      if ('"$cumulative"')
        printf("%d\t%5.1f%%\t", csum, 100.0*csum/sumh);
      printf("%d\t%5.1f%%\t", value, 100.0*value/sumh);
      for (j = 0; j < (columns-1-(8*(3+2*'"$cumulative"')))*value/maxh; j++)
        printf("*");
      print "";
}
BEGIN {
  columns = ENVIRON["COLUMNS"]+0;
  if (columns < 1)
    columns = 80;
}
{
  data = $'"$column"';
  if ('"$hasstep"') {
    data += 0;
    if (data < ('"$min"') || data > ('"$max"') ||
        (data-('"$min"'))%('"$step"') != 0)
      print "Error: invalid data: " data >"/dev/stderr";
  }
  h[data]++;
  sumh++;
  if (h[data] > maxh)
    maxh = h[data];
}
END {
  csum = 0;
  if ('"$hasstep"') {
    for (i = ('"$min"'); i <= ('"$max"'); i += ('"$step"'))
      printline(i, h[i], 1);
  } else {
    for (i in h)
      printline(i, h[i], 0);
  }
}' | sort -n

