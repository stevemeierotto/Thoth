These are the instructions on how Thoth is used by the user.

When you use /goal (or start a prompt with "Goal:"), you are activating the ExecutiveController (the "Cognitive Spine"). Here is what is happening behind the scenes
  and why it doesn't give a standard chat reply immediately:


  1. It is "Thinking" about the Plan, not Chatting
  Instead of just answering your question, the agent treats your input as a Directive. It sends your goal to the LLMPlanner, which performs a specialized "Reasoning"
  step:
   * It analyzes your goal against the RAG context (the files you dropped).
   * It generates a Multi-Step Plan (which you see in the horizontal ExecutiveStateStrip).
   * It populates the Goal Banner to "lock in" the objective.


  2. How the Model is being "Helped"
  The model is getting significant help through Directional Retrieval ($G-C$):
   * In a normal chat, the model only sees what it can "remember" or what is in the immediate prompt.
   * In Goal Mode, every time the agent executes a step, the GRAG engine specifically fishes for code chunks that bridge the gap between "what is done" and "the final
     goal."
   * This "helps" the model by ensuring the most relevant technical context is always in its "eyesight" for the specific task at hand, reducing hallucinations and
     "forgetting."


  3. Why there is no "Reply"
  In this mode, the Execution Loop takes over:
   1. The agent moves to Step 1 of the plan.
   2. It will likely call a Tool (like project_analyze or code_modify).
   3. If the tool requires Confirmation, you will see a pop-up or a button to "Approve" the action.
   4. The "Reply" is the Result: The agent's "answer" isn't text—it's the successful completion of the steps (e.g., the code being written or the file being summarized).


  How to see what it's doing:
   * Watch the State Strip: You'll see the circles change from gray to blue (Running) and then green (Success).
   * Check the GRAG Diagnostics: You'll notice the Alpha value should be higher (usually ~0.40–0.80) during a goal, showing that the "Directional Steering" is actively
     pulling in goal-related code to help the agent work.


  Summary: The lack of a text reply is normal because the agent has switched from "Chat Mode" to "Autonomous Worker Mode." It is now focused on executing the plan it
  just built for you!

## Plan L seed sentinel (do not remove)

Unique retrieval-verification token for Compose workspace seeding (Plan L L2).
Operators query this exact phrase when checking `CHAT_RAG_CONTEXT` telemetry.

THOTH_PLAN_L_SEED_SENTINEL_7f3a9c2e

