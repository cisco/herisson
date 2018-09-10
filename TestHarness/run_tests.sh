#!/bin/bash
set -e
source configure_paths.sh
(
	cd external/collectd;
	./build.sh
)
mvn package
java -jar target/TestHarness-1.0-SNAPSHOT.jar



