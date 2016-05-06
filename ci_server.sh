#!/bin/bash

make NAME=oscillo tmp/oscillo.tcp-server/tmp/server/kserverd
make NAME=spectrum tmp/spectrum.tcp-server/tmp/server/kserverd
make NAME=pid tmp/pid.tcp-server/tmp/server/kserverd
