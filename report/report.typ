#import "./template/template/template.typ": *

#show: setup-page-counting

#title_page(
  title: [Leveraging QEMU plugins for fuzzing],
  author: "Marwan Azuz",
  type: "Semester project report",
  logos: (
    image("./template/logos/EPFLlogo.svg"),
    image("./template/logos/LABlogo.svg")
  ),
  body: [
    Prof. Mathias Payer \
    Supervisor

    Florian Hofhammer \
    Supervisor
  ],
  date: datetime(year: 2025, month: 1, day: 10)
)

#dedication[
  #align(right)[
    #quote("What's reality? I don't know…", attribution: "Terry A. Davis", block: true)
  ]

  #align(center)[
    No one to dedicate this work to, except to me and the ones that beared my existence.
  ]
]

#pagebreak(weak: true)
#page-number-show
#page-number-reset

#page-title(title: "Acknowledgments")

This work would not have been possible without the amazing team at HexHive, and
the support of my friends, who have listened to me.

I would like to thank as well Florian Hofhammer for his guidance and support
throughout the project.

As well, I am scared of Prof. Mathias Payer.

_Lausanne, #datetime.today().display("[month repr:long] [day padding:none], [year]")_
#h(1fr)
Marwan

#page-title(title: "Abstract")

#set par(
  first-line-indent: 1em
)

#text(fill: red)[
  No idea to write here
]

#page-title(title: "Contents", outlined: false)

#outline(
  title: none,
  fill: none,
)

#chapter(title: "Introduction")


// Gently ease
_American-Fuzzy-Lop plus plus_ (AFL++) is a popular fuzzer that has been used to
find numerous security vulnerabilities in software. The most effective way 
to fuzz is to have coverage-guided fuzzing: when source code is available,
AFL++ provides compiler plugins to compile the target software with
instrumentation that allows feedback to the fuzzer on interesting inputs.
However, for closed-source software, this is not possible.
There are several ways to address this issue, one of them is to use CPU emulation
and virtual machines. Therefore, AFL++ can be configured to use _Quick Emulator_
(QEMU) using that has been patched to support AFL++, namely
#link("https://github.com/AFLplusplus/qemuafl")[`qemuafl`].

// Whoops, there's a problem, but wait… QEMU has a plugin system!
However, the starting point of the fork of `qemuafl`
is 4 years old
#link("https://github.com/AFLplusplus/qemuafl/commit/5c65b1f135ff09d24827fa3a17e56a4f8a032cd5")[(commit 5c65b1f)]
and having a more recent of QEMU would be beneficial. Using a fork is not ideal
as fixes and improvements in the mainline QEMU are not available.
For end-users, it is not ideal to install twice the same software, each
having different features for the same goal, and each requiring another `make all`. #link("https://www.qemu.org/2019/12/13/qemu-4-2-0/")[QEMU 4.2]
has introduced a plugin system letting users extend QEMU with custom code.
Instead of using a fork of QEMU, it would be beneficial to use the plugin system to
integrate AFL++ with QEMU, distinctively and independently from the mainline
QEMU.

// Challenge of doing it
The challenges of this project are numerous, as QEMU is a complex piece of software,
maintained by a large community. The plugin system is sometimes not well documented,
and the interaction with AFL++ or QEMU is not straightforward.
The goal of this project is to add a piece of peace to the puzzle, by providing
a plugin that can be gently added to QEMU, filling the gap between a powerful
emulator and a powerful fuzzer. How powerful and limiting a plugin is, is yet
to be read by the reader currently reading this sentence.

In this report, we present the design and implementation of a QEMU plugin that
integrates AFL++ with QEMU, with very little to no effort. We will show that
our current implementation is able to fuzz simple programs, that room for
improvements is still there and that the plugin system remains to be improved.


#chapter(title: "Background")

//The background section introduces the necessary background to understand your
//work. This is not necessarily related work but technologies and dependencies
//that must be resolved to understand your design and implementation

Fuzzing is like a lab experiment with monkeys: if they discover
that doing a certain action gives them a banana, they will keep doing it; replace
the banana by a program crash and the monkey by a fuzzer and you have a very
simple analogy of what fuzzing is.
To be more formal and to put words on the analogy, fuzzing's goal is to find
crashes. It follows a simple routine:  
generate an input to a program, run the program with the input, and
observe the behavior of the program. If it crashes, it likely indicate an issue in the
program. The end-user is then free to investigate the crash and fix the issue,
as some of these crashes can turn out to be security vulnerabilities.


However, the fuzzer might take a while
to find what action gives them the #strike[banana] crash.
To guide the fuzzer in the right direction, we forward
to the fuzzer some feedback. This feedback is usually a coverage map of the
program, that tells the fuzzer which parts of the program have been executed.
Upon finding an input that triggers a new part of the program, the fuzzer will
keep this input and try to mutate it to possibly explore more parts of the
program. Many mutators exist, and a fuzzer can be as simple as a random
mutator or as complex as a genetic algorithm.

“*How coverage is obtained?*” you might ask. Many tools out there exist but they
essentially boil down to: at desired instructions in the program, before
executing, do _something_.
The _something_ can be as simple as incrementing a counter, or as complex as 
recording the path taken by the program. The how is usually implemented with
compiler instrumentation, specific debuggers , dynamic binary instrumentation or CPU emulation.




This section is usually 3-5 pages.


#chapter(title: "Design")

Introduce and discuss the design decisions that you made during this project.
Highlight why individual decisions are important and/or necessary. Discuss
how the design fits together.

This section is usually 5-10 pages.

#chapter(title: "Implementation")

The implementation covers some of the implementation details of your project.
This is not intended to be a low level description of every line of code that
you wrote but covers the implementation aspects of the projects.

This section is usually 3-5 pages.

#chapter(title: "Evaluation")

In the evaluation you convince the reader that your design works as intended.
Describe the evaluation setup, the designed experiments, and how the
experiments showcase the individual points you want to prove.

This section is usually 5-10 pages.

#chapter(title: "Related Work")

The related work section covers closely related work. Here you can highlight
the related work, how it solved the problem, and why it solved a different
problem. Do not play down the importance of related work, all of these
systems have been published and evaluated! Say what is different and how
you overcome some of the weaknesses of related work by discussing the 
trade-offs. Stay positive!

This section is usually 3-5 pages.

#chapter(title: "Conclusion")

In the conclusion you repeat the main result and finalize the discussion of
your project. Mention the core results and why as well as how your system
advances the status quo.