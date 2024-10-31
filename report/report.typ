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
    Approved by the Examining Committee:

    Prof. Dr. sc. ETH John Doe \
    Thesis Advisor

    The External Reviewer \
    External Expert

    The Doctoral Student \
    Thesis Supervisor
  ]
)

#dedication[
  #align(right)[
    #quote("Follow the white rabbit...", attribution: "The Matrix", block: true)
  ]

  #align(center)[
    Dedicated to my pet bunny.
  ]

  The dedication is usually a short inspirational quote.
  Define your dedication here.
]

#pagebreak(weak: true)
#page-number-show
#page-number-reset

#page-title(title: "Acknowledgments")

This is where you thank those who supported you on this journey. Good examples
are your significant other, family, advisers, and other parties that inspired
during this project. Generally this section is about 1/2 page to a page.

_Lausanne, #datetime.today().display("[month repr:long] [day padding:none], [year]")_
#h(1fr)
The Student

#page-title(title: "Abstract")

#set par(
  first-line-indent: 1em
)

The FooSystem tool enables lateral decomposition of a multi-dimensional
flux compensator along the timing and space axes.

The abstract serves as an executive summary of your project.
Your abstract should cover at least the following topics, 1-2 sentences for
each: what area you are in, the problem you focus on, why existing work is
insufficient, what the high-level intuition of your work is, maybe a neat
design or implementation decision, and key results of your evaluation.

#page-title(title: "Résumé")

For a doctoral thesis, you have to provide a French translation of the English
abstract. For other projects this is optional.

#page-title(title: "Contents", outlined: false)

#outline(
  title: none,
  fill: none,
)

#chapter(title: "Introduction")

The introduction is a longer writeup that gently eases the reader into your
thesis. Use the first paragraph to discuss the setting. In the second paragraph
you can introduce the main challenge that you see. The third paragraph lists why
related work is insufficient. The fourth paragraph introduces your thesis
statement. Think how you can distill the essence of your thesis into a single
sentence. The seventh paragraph will highlight some of your results. The eights
paragraph discusses your core contribution.

This section is usually 3-5 pages.

#chapter(title: "Background")

The background section introduces the necessary background to understand your
work. This is not necessarily related work but technologies and dependencies
that must be resolved to understand your design and implementation.

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