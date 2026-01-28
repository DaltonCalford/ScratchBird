# How this all started

This page explains why ScratchBird exists and how I ended up making the choices that shaped it.

---

> **Timeline (high level)**
> 
> - **1995** - Interbase ships with Delphi and Borland tools; I start using it daily.
> - **2000** - Between 1999 to Now - Interbase becomes FirebirdSQL and is open source
> - **2025**- Market changes resulted in my layoff 
> - **AI breakthrough era** - Free Time from Layoff + new AI coding tools; I use them to learn Python.
> - **Firebird 6 deep dive** - I ask AI to map the codebase and compare code vs comments.
> - **ScratchBird born** - One-week migration to a renamed project; clean compile.
> - **First feature win** - Linux trusted authentication based on the Windows code.
> - **Guardrails moment** - A risky remote-query experiment proves AI needs strict constraints.
> - **Reset and rebuild** - Old work archived; specs-first development becomes the rule.
> - **Dec 2025** - Codex joins Claude, Gemini-cli, and Ollama in the verification loop.
> - **Jan 2026** - Kimi K2.5 enters the mix for use with management software (ScratchRobin)

## The long relationship with Firebird

I first met Firebird (then Interbase) in 1995. It shipped with Delphi and the Borland tools I was using every day. It was stable, fast, and single-file, which made it easy to manage and hard to forget. That simplicity stuck with me: the database felt dependable and approachable. When it went open-source I wanted to contribute, but the codebase felt like a maze of C-to-C++ migrations, many authors, and hidden complexity. I did not want to be the person who guessed wrong and broke something I respected.

For years I admired it from a distance and told myself I would eventually figure it out. I kept the idea on the shelf, waiting for the right time and the right tools. I knew I needed a better map first.

---

## The catalyst

Two things changed everything at once: I was laid off, and AI coding tools like Gemini and Claude were suddenly good enough to be useful. I used them to learn Python, and that showed me a new path. If AI could help me learn quickly, maybe it could help me navigate a huge, unfamiliar codebase too. That was the moment I decided to try.

That gave me a mission: pull down Firebird 6, map the code, and finally learn it well enough to contribute. I wanted a clear map before touching anything, not another blind refactor. The goal was understanding first.

I also started documenting the journey on LinkedIn. Part of it was to learn how that platform works, and part of it was the reality of a job search. Each article linked back to the project and helped me stay honest about what was real progress versus wishful thinking. I may write a follow-up article once the next phase settles.

---

## Why "ScratchBird"

I called it **ScratchBird** because that is exactly how I work when I am learning something new: I start with scratch code. It is rough, disposable, and never meant for production. It is where I prove to myself that I really understand the moving parts.

This project is one big scratchpad. I wanted the name to say two things at once:

- **Homage** to Firebird and the database that started this whole obsession.
- **Clear separation** that this is not Firebird, and it should never be mistaken for it.

I also have a 25-year itch to make Firebird do things I always wished it could. The goal is not to replace it, but to explore ideas in a clean sandbox and, if I am lucky, see some of those ideas make their way back to the original project.

With AI handling the heavy lifting, the initial migration took about a week instead of months, and I got a clean compile. That was the start of ScratchBird, and it gave me confidence that this approach could work. I kept reminding myself that this was scratch code, a place to learn without pretending it was production.

---

## When the refactor hit the wall

Not long after those early LinkedIn articles, it became obvious that the AIs of the time could not safely refactor the original codebase. Firebird had custom tools, unique solutions, and decades of C-to-C++ history, and the models did not understand the full picture. They sounded confident even when they were guessing, and that is not a safe place to be when you are touching core engine code.

I had to decide whether to keep wrestling with that reality. I chose to keep going, but with a new approach: treat AI like a powerful tool that needs a strict process. That decision shaped everything that followed.

---

## Early wins and hard lessons

My first task was small: bring trusted authentication to Linux by following the Windows implementation. AI helped me research, code, and document it, and a two-day task became a few hours. That felt like a breakthrough and showed me where AI could shine.

The next task was ambitious: remote database query support. I gave the AI a big idea and too much freedom. It changed code all over the project, invented APIs, and produced something unsafe and non-standard. It was fast, but it was wrong. That swing from win to mess is why I tightened the process.

That failure taught the most important lesson of this project: AI needs guardrails. It is a powerful assistant, not a decision-maker. That became the guiding rule.

---

## Building the guardrails

When I started, AI did not have features that are now standard (like skills). The context window was small, and when a model did not know how to do something, it would keep going and waste time on the wrong work. That behavior is dangerous in a database engine, so I had to design a process that would catch it.

So I treated AI like any other programming language: define goals, compile specifications, build plans, create processes, and make sure I had ways to detect when the AI was wrong. It was about reducing risk and making progress repeatable. I wanted proof, not optimism.

The core pattern became cross-checking:

- One AI reads specs and writes plans and test specifications.
- A second AI reads the tests and builds the tools or harnesses.
- A third AI uses those tools to verify whether the code matches the tests.

Sometimes that trio was Claude, Gemini-cli, and Ollama on my home lab. Later I added Codex (December 2025). With that process in place, I archived the old work in git history and began again with a clean, spec-driven foundation. The reset was not a failure, it was a decision to build on something solid. I would rather be slow and right than fast and wrong.

This turned ScratchBird into a real experiment: can AI handle a large, complex project without failing, and can I manage it to completion?

---

## The testing turning point

After that near-miss, I focused on safety. The fastest way to build safe software was to build a test harness. If I could not test changes, I could not trust them. I needed a safety net for both me and the AI.

Instead of asking AI to rewrite thousands of SQL tests, I asked it to build a tool that could convert legacy suites into command-line regression tests. That shift paid off immediately: I had repeatable tests and real confidence that new work was not breaking old behavior. That confidence is what makes momentum possible.

---

## Why ScratchBird is built the way it is

These experiences shaped the core philosophy. Each item is a choice I made to reduce risk and keep the project honest. If a rule felt heavy, it was usually because I had already been burned without it:

- **Architecture clarity** over shortcuts. I chose strict boundaries between the engine, parsers, and wire protocols so I can trust where responsibility ends.
- **MGA correctness** over MVCC lookalikes. I anchored the engine to Firebird's Multi-Generational Architecture because it is proven and predictable.
- **Compatibility without betrayal.** I require emulated parsers to match their dialects and avoid ScratchBird-only features, or the illusion breaks.
- **Tests as armor.** I made every major change earn a regression path, because speed without safety is a trap.
- **Process discipline.** I rely on specs, plans, and independent verification to keep the AI honest.
- **Human validation.** AI can move fast, but the final 5 percent is still expert judgment.

---

## What changed for me

ScratchBird started as a dream to contribute to a beloved database. It turned into a full re-think of my role as a developer: less pure coder, more architect, validator, and guide. I am the one who sets the constraints and decides when the machine is wrong.

That is why ScratchBird is not just a rewrite. It is a deliberate attempt to build the database I always wanted, with the discipline and guardrails that make long-term progress possible.
