ARG base_tag=latest
ARG scratch_img=ubuntu
ARG scratch_img_tag=latest
FROM ghcr.io/testillano/kafkatools_builder:${base_tag} as builder
MAINTAINER testillano

LABEL testillano.kafkatools.description="C++ kafka-tools image"

COPY . /code
WORKDIR /code

ARG make_procs=4
ARG build_type=Release

# We could duplicate from local build directory, but prefer to build from scratch:
RUN cmake -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs}

FROM ${scratch_img}:${scratch_img_tag}
ARG build_type=Release
COPY --from=builder /code/build/${build_type}/bin/kafka-producer /opt/
COPY --from=builder /code/build/${build_type}/bin/udp-server-kafka-producer /opt/

# We add curl & jq for helpers.src
# Ubuntu has bash already installed, but vim is missing
ARG base_os=ubuntu
RUN if [ "${base_os}" = "alpine" ] ; then apk update && apk add bash curl jq && rm -rf /var/cache/apk/* ; elif [ "${base_os}" = "ubuntu" ] ; then apt-get update && apt-get install -y vim curl jq && apt-get clean ; fi

ENTRYPOINT ["/opt/kafka-producer"]
CMD []

