#!/usr/bin/env bash

head_file_location_1=$1"/include/"
head_file_location_2=''
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        head_file_location_2=$head_file_location_1'linux/'
elif [[ "$OSTYPE" == "darwin"* ]]; then
        head_file_location_2=$head_file_location_1'darwin/'
fi
gcc -I$head_file_location_1 -I$head_file_location_2 -shared -o gar_test.so -fPIC garbage_collection_tracker.c
echo 'succesfully created gar_test.so file at '$PWD
echo 'use the .so file created as an agent to the jvm example
      java -agentpath:./gar_test.so=/Users/$user_name/Desktop/mleak.log,GB Test'