#!/bin/bash

BASHSCRIPTDIR=$(cd "$(dirname "$0")" || exit; pwd)
EXTDIR="${BASHSCRIPTDIR}/_externals"
#GLMVER="0.9.9.8"
GLMVER="efec5db081e3aad807d0731e172ac597f6a39447"
SDLVER="release-2.26.4"
IMGUIVER="4789c7e485244aa6489f89dbb03b19d4ad0ea1ec"

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

mkdir -p "${EXTDIR}" || exit 1
pushd "${EXTDIR}" || exit 1
checkout_revision "https://github.com/g-truc/glm"       "${GLMVER}"
checkout_revision "https://github.com/libsdl-org/SDL"   "${SDLVER}"
checkout_revision "https://github.com/ocornut/imgui"    "${IMGUIVER}"
popd

