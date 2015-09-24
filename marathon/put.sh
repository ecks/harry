#!/usr/bin/env bash
MARATHON=http://10.0.2.78:8080

if [[ -z $1 ]];
then
  echo "need AppId"
  echo $1
else
  echo "executing curl PUT command for $1"
  curl -X PUT $MARATHON/v2/apps/$1 -d @$1.json.put -H "Content-type: application/json"
fi
