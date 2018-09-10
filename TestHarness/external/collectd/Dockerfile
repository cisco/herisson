FROM ubuntu:16.04
MAINTAINER Alessandro Duminuco <aduminuc@cisco.com>

ADD pkg.ci.collectd.org.list /etc/apt/sources.list.d/pkg.ci.collectd.org.list
RUN apt-get update
RUN apt-get install --allow-unauthenticated -y collectd vim

ADD collectd.conf /etc/collectd/collectd.conf
ADD my_types.db /etc/collectd/my_types.db

EXPOSE 5432
EXPOSE 9103

CMD collectd -C /etc/collectd/collectd.conf -f


