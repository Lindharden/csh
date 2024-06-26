project('csh', 'c', subproject_dir: 'lib', default_options: [
	'csp_ippc:slash=true', 
	'buildtype=debug', 
	'c_std=gnu11', 
	'b_lto=false',
	'default_library=static',
	'slash:builtins=true',
	'csp:packet_padding_bytes=42', 
	'csp:buffer_count=1000',
	'csp:buffer_size=2048',
	'csp:conn_max=20',
	'csp:conn_rxqueue_len=1000',
	'csp:qfifo_len=1000',
	'csp:rdp_max_window=1000',
	'csp:port_max_bind=16',
	'csp:use_rtable=true',
	'param:have_fopen=true', 
	'param:slash=true', 
	'param:commands=true',
	'param:scheduler=true',
	'param:commands_client=true',
	'param:scheduler_client=true',
	'param:collector=false', 
	'param:list_dynamic=true'])

add_global_arguments('-I../include', language: 'c')

curl_dep = dependency('libcurl', required: false)
if not curl_dep.found()
  error('libcurl not found! Please install libcurl4-openssl-dev or the appropriate package for your system.')
endif

zmq_dep = dependency('libzmq', required: false)
if not zmq_dep.found()
  error('libzmq not found! Please install libzmq3-dev or the appropriate package for your system.')
endif

# .as_link_whole() prevents the linker from discarding functions/symbols that appear unused.
# Specifically; functions/symbols that are unused during static linking,
# but that may be called after dynamic linking by APMs.
csp_dep = dependency('csp', fallback: ['csp', 'csp_dep'], required: true).as_link_whole()
slash_dep = dependency('slash', fallback: ['slash', 'slash_dep'], required: true).as_link_whole()
param_dep = dependency('param', fallback: ['param', 'param_dep'], required: true).as_link_whole()
lua_dep = dependency('lua5.4', required: false)
csp_ippc_dep = dependency('csp_ippc', fallback: ['csp_ippc', 'csp_ippc_dep'], required: true).as_link_whole()

csh_sources = [
	'src/main.c',
	'src/slash_apm.c',
	'src/slash_csp.c',
	'src/slash_eth.c',
	'src/csp_debug_param.c',
	'src/base16.c',
	'src/param_sniffer.c',
	'src/prometheus.c',
	'src/resbuf_dump.c',
	'src/stdbuf_mon.c',
	'src/stdbuf_client.c',
	'src/randombytes.c',
	'src/csp_scan.c',
	'src/sleep_slash.c',
	'src/spaceboot_slash.c',
	'src/nav.c',
	'src/serial.c',
	'src/known_hosts.c',
	'src/hk_param_sniffer.c',
	'src/lock.c',
	'src/csp_init_cmd.c',
	'src/vts.c',
	'src/walkdir.c',
    'src/victoria_metrics.c', 
    'src/loki.c', 
]

if lua_dep.found()
	csh_sources += 'src/lua.c'
endif

csh_sources += vcs_tag(input: files('src/version.c.in'), output: 'version.c', command: ['git', 'describe', '--always', '--dirty=+'])


csh = executable('csh', csh_sources,
	dependencies : [slash_dep, csp_dep, param_dep, lua_dep, curl_dep, csp_ippc_dep],
	link_args : ['-Wl,-Map=csh.map', '-lm', '-Wl,--export-dynamic', '-ldl'],  # -ldl is needed on ARM/raspbarian
	install : true,
)

custom_target('size', output: ['dummy.txt'], command: [find_program('size'), csh.full_path()], depends: csh, build_by_default: true)

zmqproxy_sources = ['src/zmqproxy.c']
zmqproxy = executable('zmqproxy', zmqproxy_sources,
	dependencies : [csp_dep, zmq_dep],
	install : true,
)

spacebridge_sources = ['src/spacebridge.c']
spacebridge = executable('spacebridge', spacebridge_sources,
	dependencies : [csp_dep],
	install : true,
)

install_data('init/caninit', install_dir : get_option('bindir'))
