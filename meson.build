project('SADomains', 'cpp', version: '1.0')

compiler = meson.get_compiler('cpp')

add_project_arguments('-Wno-pedantic', language: 'cpp')
add_project_arguments('-Wno-missing-field-initializers', language: 'cpp')

LocksProj = subproject('Locks')
Locks = LocksProj.get_variable('Locks')

SADomainsLib = static_library(
	'SADomains',
	sources: [
		'Domains.cpp',
		'DomainCondVars.cpp',
	],
	include_directories: [
		'SADomains'
	],
	dependencies: [
		compiler.find_library('be'),
		Locks
	],
)

SADomains = declare_dependency(
	link_with : SADomainsLib,
	include_directories: [
		'.',
		'/boot/system/develop/headers/private/shared'
	],
	dependencies: [Locks]
)
