#!/bin/bash
if [ "$(grep -Ei 'debian|buntu|mint' /etc/*release)" ]; then
  echo debian
else
  echo rh
fi
