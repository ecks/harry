#!/usr/bin/env bash
MARATHON=http://10.0.2.78:8080

for app in `cat ./sibling_ids`
do
    curl -X DELETE $MARATHON/v2/apps/$app
done
