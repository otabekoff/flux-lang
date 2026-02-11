# Enhanced Prompt for ChatGPT 4.1 Agent

Always obey these rules:

You are working on this project using the instructions in **ROADMAP.md**.

## General Workflow

1. Process the project **phase by phase, section by section, and task by task** according to `ROADMAP.md`.
2. For each task:
    - Implement it fully from start to finish completely.
    - Ensure everrything is end to end complete.
    - Ensure the project builds successfully.
    - Ensure no problems left in the editor problems tab.
    - Ensure all related tests pass.
    - Don't be lazy.
    - Dont' put anything to later. Do everything in place.

3. Only mark a task as **completed** when:
    - Implementation is finished.
    - Build succeeds.
    - All tests pass.
    - No related issues remain.

Don't forget to track and ensure the set the current correct and clear progress always in todos and ROADMAP.md.

If a task is:

- Missing requirements → clearly mark as **incomplete**.
- Partially implemented → mark as **partially complete**.
- Blocked → describe the blocker and what is needed.

---

## Implementation Rules

- Do not skip tasks.
- Do not leave unfinished or broken code.
- Do not postpone required changes.
- Do not simplify tests just to make them pass.
- Tests define the correct behavior.
- Don't escape from big changes.

If a task requires:

- Refactoring
- Large architectural changes
- Multiple steps

Then:

- Perform the changes immediately.
- If too large, break into smaller steps but make sure to do completely end-to-end.
- Complete all steps end-to-end before marking the task done.

So you are free to:

- Add
- Create
- Read
- Update
- Delete
- Edit
- Upgrade
- Fix
- Bug fix
- Implement
- Make changes
- Remove
- Insert
- Do
- Test
- Check
- Improve
- Format
- Implement
- Refactor
- Research
- Optimize
- Download
- Setup
- Organize

---

## Code Quality

- Ensure the code compiles without errors.
- Fix warnings where reasonable.
- Format code properly.
- Remove dead or obsolete code.
- Keep the project consistent and clean.
-

You are free to:

- Refactor code.
- Rename components.
- Reorganize modules.
- Improve architecture when needed.

---

## Documentation and Tracking

- Use `ROADMAP.md` to track progress.
- Update task statuses as you work.
- Keep everything in sync with the docs and codebase.
- If functionality changes, update relevant documentation.
- If documentation is missing or outdated:
    - Create or update files in the `docs/` folder.

- Keep TODOs and docs in sync with the codebase.

---

## Research and External Information

- If something is unclear or outdated:
    - Search for up-to-date information.
    - Search for up-to-date information on the internet.
    - Use reliable, modern sources.

- Apply best practices from modern programming language design.

---

## Professional Standard

Behave like a **professional programming language creator**:

- Make correct architectural decisions.
- Prefer long-term maintainability over short-term hacks.
- Ensure consistency across the entire codebase.

---

## Conventional commits

We use [conventional commits](https://www.conventionalcommits.org/en/v1.0.0/) to generate changelogs and release notes.

- Commits should be prefixed with the type of change (feat, fix, docs, etc.).
- Commits should be written in present tense.

After each successful task completion, you should run `git add .` and `git commit` to generate a new commit.
