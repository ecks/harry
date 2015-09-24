#!/usr/bin/env bash
MARATHON=http://10.0.2.78:8080

for i in *.json
do
    curl -X POST $MARATHON/v2/apps -d @$i -H "Content-type: application/json"
done
