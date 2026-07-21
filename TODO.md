graphics:
- [x] double buffering
- [x] transparency
- [x] blending
- [x] circles
- [ ] lines
- [ ] sprite embedding
- [ ] sprite drawing
- [ ] move draw\_char draw\_text to other location
- [x] draw_text_immediate for asserts and term::print

fonts:
- [ ] embedding ttfs
- [ ] simple ttf drawing

sound:
- [ ] figure out what to actually do here

base layer:
- [x] timers
- [x] interrupts
- [x] PIC
- [ ] APIC instead of PIC if available later on
- [ ] keyboard handling
- [ ] mouse handling
- [ ] multicore
- [x] serial
- [x] fix early asserts (term must be initialized for asserts to output anything currently and we have asserts that are before the term::initialize)
- [ ] move all __variables to .cc files
- [ ] implement cpuid (https://wiki.osdev.org/CPUID)
- [ ] remove term declarations in halt.hh instead of that do a function pointer for term::print, that would decouple the two systems completely (could even do something like a list of print function pointers that would get called after halt fires)

utils:
- [ ] todo list generator (gather notes from source code and generate a stable
      list that doesn't move stuff around, handles marking stuff as done, etc...)
- [ ] metaprogramming framework - some metaprogram that would allow for example
      to simply embed stuff by doing `auto* img = @embed(path_to_img);`, many more
      features could be added here
