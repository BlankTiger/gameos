[ ] - not done
[/] - cancelled
[x] - done
[-] - in progress

graphics:
- [x] double buffering
- [x] transparency
- [x] blending
- [x] circles
- [x] lines
- [x] sprite embedding
- [x] sprite drawing
- [x] move draw\_char draw\_text to other location
- [x] draw_text_immediate for asserts and term::print
- [x] think of a better way of storing b,g,r channels, so we can both load rgb and use bgr elsewhere
- [ ] draw tetris
- [ ] separate core for graphics

fonts:
- [ ] embedding ttfs
- [ ] simple ttf drawing

sound:
- [ ] figure out what to actually do here
- [ ] 8-bit slowly drifting

base layer:
- [x] timers
- [x] interrupts
- [x] PIC
- [x] APIC instead of PIC if available later on
- [ ] keyboard handling
- [ ] mouse handling
- [x] multicore
- [x] serial
- [x] fix early asserts (term must be initialized for asserts to output anything currently and we have asserts that are before the term::initialize)
- [/] move all __variables to .cc files
- [x] implement cpuid (https://wiki.osdev.org/CPUID)
- [x] remove term declarations in halt.hh instead of that do a function pointer for term::print, that would decouple the two systems completely (could even do something like a list of print function pointers that would get called after halt fires)
- [ ] fix booting on real hardware
- [x] tests (gtest?)
- [ ] stack unwinding (callstack on assert)
- [x] save floating point registers on an interrupt
- [x] save SIMD registers on an interrupt
- [x] get random numbers for the seed in random.hh
- [x] should Array_View and Static_Array do bounds checking or not? They should
- [x] String_Builder
- [x] keep allocator on structures that allocate
- [x] temporary storage + tprint/tcopy/talloc
- [x] string RAII vs allocator swap (option 2; see docs/string-allocator-plan.md)
- [x] test defer freeing string (test that it is really freed)
- [ ] fix halt::Backend (look at the todo there)
- [ ] mark unallocated pages as read only
- [x] RAII push_allocator(Allocator* allocator) that sets an allocator for the current scope
- [ ] get rid of ensure_allocator from Array, there should always be a valid allocator on a live heap backed Array
- [ ] Debug_Allocator try to match as much of the zigs debug allocator as possible
- [ ] MarkAndRestore for temporary allocations?
- [ ] realloc on Allocator
- [ ] query_capabilities on Allocator
- [ ] set_temporary_allocator (also make it wrappable with Debug_Allocator)
- [ ] extract programmable_interval_timer.hh from time.hh
- [x] extract interrupts.hh vector ids to a constants.hh type of file (currently they are duplicated throughout the file)
- [ ] somehow deduplicate all the declarations that are made per an interrupt vector in interrupts.hh
- [ ] make the serial_port.hh implementation universal (nowadays serial is usually only available through PCI extension cards and we would have to enumarate those)
- [ ] Send a deassert in SMP startup code (spec says it should be there)
- [ ] create a Thread_Config structure that user of threads.hh can use to configure threads, similar thing should be also possible to do for a core
- [ ] Currently BSP is treated differently from all the
      additional cores we manage to get. If there is more than 1 then we don't
      ever switch it's task. That however is a detail that the user of this
      library probably wants to control so we have to figure out a way to switch
      the core configuration in a way that doesn't hinder performance and is
      easy/intuitive to use.


utils:
- [ ] todo list generator (gather notes from source code and generate a stable
      list that doesn't move stuff around, handles marking stuff as done, etc...)
- [x] metaprogramming framework - some metaprogram that would allow for example
      to simply embed stuff by doing `auto* img = @embed(path_to_img);`, many more
      features could be added here
- [x] make the preprocessor output debug symbols based on the real paths, not the generated paths
- [x] pretty enum printing (autogenerate value -> pretty enum value as string mappings)
- [ ] generate enums out of defines with @enum. Or maybe the other way around, generate defines from an enum, idk yet (look at interrupts_constants.hh)
- [ ] unfuck preprocessor (tokenizer?)
- [ ] get rid of cmake (build.cc)
