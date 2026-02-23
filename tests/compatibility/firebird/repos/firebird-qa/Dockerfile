FROM ubuntu:22.04

RUN \
	apt-get update && \
	\
	DEBIAN_FRONTEND=noninteractive apt-get -y install \
		libicu70 \
		libncurses5 \
		libtommath1 \
		python3 \
		python3-pip \
		python3-venv \
		sudo && \
	\
	rm -rf /var/lib/apt/lists/* && \
	\
	ln -s /usr/lib/x86_64-linux-gnu/libtommath.so.1 /usr/lib/x86_64-linux-gnu/libtommath.so.0

ARG UID=1000

COPY pyproject.toml /qa-run/

RUN \
	useradd -m -u $UID user -G sudo && \
	groupadd firebird && \
	useradd --non-unique -M -b /opt -s /sbin/nologin -g firebird -u $UID firebird && \
	usermod -G sudo firebird && \
	echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers && \
	\
	mkdir /qa-out && \
	chown -R user:user /qa-out && \
	chown -R firebird:firebird /qa-run && \
	cd /qa-run && \
	ln -s /qa-out out && \
	python3 -m pip install pipx

USER user

RUN \
	cd /qa-run && \
	pipx ensurepath && \
	pipx install --preinstall pytest-md-report --preinstall pytest-timeout --include-deps firebird-qa

ENV PATH=/opt/firebird/bin:/home/user/.local/bin:$PATH
ENV TERMINFO_DIRS=/lib/terminfo
ENV LD_LIBRARY_PATH=/opt/firebird/lib

ENTRYPOINT ["/qa/docker/run.sh"]
