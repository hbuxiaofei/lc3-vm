#!/usr/bin/env bash

cd $(dirname $0)

INSTALL_DIR="/usr/local/bin"

LC3_DIR="lcc-1.3"

[ ! -d $INSTALL_DIR ] && mkdir -p $INSTALL_DIR

[ -d ${INSTALL_DIR}/${LC3_DIR} ] && rm -rf ${INSTALL_DIR}/${LC3_DIR}

cp -rf ${LC3_DIR} $INSTALL_DIR/

chmod +x ${INSTALL_DIR}/${LC3_DIR}/install/*


