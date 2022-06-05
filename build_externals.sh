#!/bin/bash

BASHSCRIPTDIR=$(cd "$(dirname "$0")" || exit; pwd)
DEPENDENCIESDIR="${BASHSCRIPTDIR}/_externals"
GLMVER="0.9.9.8"

checkout_revision(){
    REMOTE_REPO=$1
    REVISION=$2
    REPO=$(basename "${REMOTE_REPO}")
    if [[ -d "${REPO}" ]] ; then
        cd "${REPO}" || exit
        git checkout -
        git pull
    else
        git clone "${REMOTE_REPO}"
        cd "${REPO}" || exit
    fi
    git checkout "${REVISION}"
    cd .. || exit
}

mkdir -p "${DEPENDENCIESDIR}" || exit 1
pushd "${DEPENDENCIESDIR}" || exit 1
checkout_revision "https://github.com/g-truc/glm"   "${GLMVER}"
popd

