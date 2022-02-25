== Building in Docker from Ubuntu

=== Requirements (Identified as software that is not pre-installed in Multipass Ubuntus)
* Docker
* Docker Buildx (https://docs.docker.com/buildx/working-with-buildx/)

=== Docker installation
* `sudo apt update`
* `sudo apt-get install apt-transport-https ca-certificates curl software-properties-common`
* `curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -`
* `sudo add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu  $(lsb_release -cs)  stable"`
* `sudo apt-get install docker-ce`

=== Building steps
* `sudo ./ci/run_envoy_docker.sh './ci/do_ci.sh bazel.release.server_only'`
* `sudo docker buildx build --platform linux/amd64 -f ci/Dockerfile-envoy -t envoy .`
