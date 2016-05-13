FROM jeanminet/tcp-server

ENV work_dir /__code

WORKDIR $work_dir/
COPY . $work_dir/