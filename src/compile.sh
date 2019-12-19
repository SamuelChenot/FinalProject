#!/bin/bash
# Compiles the project to a 'robot' binary.
# Usage: ./compile.sh [version] in the form 'v1' 'v2' or 'v3'

COMPILER="g++"
FLAGS="-Wall"
LIBS="-lm -lGL -lglut -lpthread"
NAME="robots"

if [[ $1 == "v1" ]] ; then
	FILES="robotsV1.cpp gl_frontEnd.cpp"
elif [[ $1 == "v2" ]] ; then
	FILES="robotsV2.cpp gl_frontEnd.cpp"
elif [[ $1 == "v3" ]] ; then
	FILES="robotsV3.cpp gl_frontEnd.cpp"
else
	FILES="robotsV1_SamFixesHisBotchedCode.cpp gl_frontEnd.cpp"
fi

$COMPILER $FLAGS $FILES $LIBS -o $NAME
