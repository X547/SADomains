project('SADomains', 'cpp', version: '1.0')

compiler = meson.get_compiler('cpp')

LocksLib = static_library(
	'Locks',
	sources: [
		'../Locks/Mutex.cpp',
		'../Locks/RecursiveLock.cpp',
		'../Locks/ConditionVariable.cpp',
		'../Locks/Sem.cpp',
		'../Locks/Timer.cpp',
	],
	include_directories: [
		'/boot/system/develop/headers/private/shared',
		'/boot/system/develop/headers/private/system',
		'/boot/system/develop/headers/private/system/arch/x86_64'
	],
	dependencies: [
		compiler.find_library('be')
	],
)

Locks = declare_dependency(
	link_with : LocksLib,
	include_directories: ['/boot/system/develop/headers/private/shared'],
)

SADomainsLib = static_library(
	'SADomains',
	sources: [
		'Domains.cpp'
	],
	dependencies: [
		compiler.find_library('be'),
		Locks
	],
)

SADomains = declare_dependency(
	link_with : SADomainsLib,
	include_directories: ['/boot/system/develop/headers/private/shared'],
)

executable('Tests', sources: ['Tests/Tests.cpp'], dependencies: [SADomains, compiler.find_library('be')], install: true)
executable('Tests2', sources: ['Tests/Tests2.cpp'], dependencies: [SADomains, compiler.find_library('be')], install: true)
executable('Tests3', sources: ['Tests/Tests3.cpp'], dependencies: [Locks, SADomains, compiler.find_library('be')], install: true)
executable('TestsCustomThread', sources: ['Tests/TestsCustomThread.cpp'], dependencies: [SADomains, compiler.find_library('be')], install: true)
executable('TestsSync', sources: ['Tests/TestsSync.cpp'], dependencies: [SADomains, compiler.find_library('be')], install: true)
executable('TestsSync2', sources: ['Tests/TestsSync2.cpp'], dependencies: [SADomains, compiler.find_library('be')], install: true)
executable('Tests4', sources: ['Tests/Tests4.cpp'], dependencies: [SADomains, compiler.find_library('be')], install: true)
executable('TestsDeadlock', sources: ['Tests/TestsDeadlock.cpp'], dependencies: [Locks, SADomains, compiler.find_library('be')], install: true)
executable('TestsGui', sources: ['Tests/TestsGui.cpp'], dependencies: [Locks, SADomains, compiler.find_library('be')], install: true)
executable('TestsAsync1', sources: ['Tests/TestsAsync1.cpp'], dependencies: [Locks, SADomains, compiler.find_library('be')], install: true)
executable('TestsTimer', sources: ['Tests/TestsTimer.cpp'], dependencies: [Locks, SADomains, compiler.find_library('be')], install: true)
executable('TestsDelete', sources: ['Tests/TestsDelete.cpp'], dependencies: [Locks, SADomains, compiler.find_library('be')], install: true)
