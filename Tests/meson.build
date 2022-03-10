project('SADomainsTests', 'cpp', version: '1.0')

compiler = meson.get_compiler('cpp')

LocksProj = subproject('Locks')
Locks = LocksProj.get_variable('Locks')

SADomainsProj = subproject('SADomains')
SADomains = SADomainsProj.get_variable('SADomains')

libbe = compiler.find_library('be')


executable('Tests', sources: ['Tests.cpp'], dependencies: [SADomains, libbe], install: true)
executable('Tests2', sources: ['Tests2.cpp'], dependencies: [SADomains, libbe], install: true)
executable('Tests3', sources: ['Tests3.cpp'], dependencies: [Locks, SADomains, libbe], install: true)
executable('TestsCustomThread', sources: ['TestsCustomThread.cpp'], dependencies: [Locks, SADomains, libbe], install: true)
executable('TestsSync', sources: ['TestsSync.cpp'], dependencies: [SADomains, libbe], install: true)
executable('TestsSync2', sources: ['TestsSync2.cpp'], dependencies: [SADomains, libbe], install: true)
executable('Tests4', sources: ['Tests4.cpp'], dependencies: [SADomains, libbe], install: true)
executable('TestsDeadlock', sources: ['TestsDeadlock.cpp'], dependencies: [Locks, SADomains, libbe], install: true)
executable('TestsGui', sources: ['TestsGui.cpp'], dependencies: [Locks, SADomains, libbe], install: true)
executable('TestsAsync1', sources: ['TestsAsync1.cpp'], dependencies: [Locks, SADomains, libbe], install: true)
executable('TestsAsync2', sources: ['TestsAsync2.cpp'], dependencies: [Locks, SADomains, libbe], install: true)
executable('TestsTimer', sources: ['TestsTimer.cpp'], dependencies: [Locks, SADomains, libbe], install: true)
executable('TestsDelete', sources: ['TestsDelete.cpp'], dependencies: [Locks, SADomains, libbe], install: true)
executable('TestsPointers', sources: ['TestsPointers.cpp'], dependencies: [Locks, SADomains, libbe], install: true)
executable('TestsSpeed', sources: ['TestsSpeed.cpp'], dependencies: [Locks, SADomains, libbe], install: true)
executable('TestsCondVar', sources: ['TestsCondVar.cpp'], dependencies: [Locks, SADomains, libbe], install: true)

executable('TestsSyncobj', sources: ['TestsSyncobj.cpp', 'Syncobj.cpp'], dependencies: [Locks, SADomains, libbe], install: true)
