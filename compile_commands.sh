#!/bin/bash
DIR=$(pwd)
for FILE in $*; do
	REALPATH=$(realpath ${TOPDIR}/${FILE} 2>/dev/null)
	if [ "$REALPATH" = "" ] ; then
		REALPATH=$(realpath ${FILE} 2>/dev/null)
	fi

	echo "{"
	echo "\"directory\": \"$DIR\""

	if [[ $REALPATH =~ .*\.cpp ]] ; then
		OUTFILE=${DIR}/${FILE%.cpp}.o
		echo "\"command\": \"$CXX -c $CXXFLAGS -o $OUTFILE $REALPATH\""
	else
		OUTFILE=${DIR}/${FILE%.c}.o
		echo "\"command\": \"$CC -c $CFLAGS -o $OUTFILE $REALPATH\""
	fi

	echo "\"file\": \"$REALPATH\""
	echo "},"
done
