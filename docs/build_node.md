# BUILD_NODE.md

Purpose: Guide Gemini (or another coding agent) through implementing the
NODE runtime harness in small, verifiable steps.

Rules for the coding agent: - Only perform ONE step at a time. - After
finishing a step, STOP and output: **"Waiting to continue."** - Do NOT
move to the next step automatically. - Each step must compile or run
before continuing. - If a step fails, fix it before proceeding.

------------------------------------------------------------------------

# PHASE 1 --- Create Core NODE Structure

## Step 1.1 --- Create NODE Directory

Create a new module or folder called:

/core/node

Files to create:

Node.h Node.cpp

Goal: Define the base NODE runtime container.

After completion print: Waiting to continue.

------------------------------------------------------------------------

## Step 1.2 --- Define Node Class Skeleton

Create a basic Node class.

Node.h should contain:

class Node { public: Node(); \~Node();

    void initialize();
    void execute();

private:

};

No functionality yet. Only compile-safe structure.

After completion print: Waiting to continue.

------------------------------------------------------------------------

## Step 1.3 --- Implement Empty Functions

Implement the functions in Node.cpp.

They should currently only log:

Node initialized Node executed

Do not add logic yet.

Compile test required.

After completion print: Waiting to continue.

------------------------------------------------------------------------

# PHASE 2 --- Define Node Types

## Step 2.1 --- Create NodeType Enum

Add:

enum class NodeType { TOOL, LLM, RETRIEVAL, CONTROL, CUSTOM };

Attach NodeType to Node class.

After completion print: Waiting to continue.

------------------------------------------------------------------------

## Step 2.2 --- Add Node Metadata

Extend Node with:

id name type

Example:

std::string id; std::string name; NodeType type;

Constructor should accept these.

After completion print: Waiting to continue.

------------------------------------------------------------------------

# PHASE 3 --- Node Input / Output

## Step 3.1 --- Define Input Structure

Add a JSON input container.

Use:

nlohmann::json input;

This allows nodes to receive arbitrary data.

After completion print: Waiting to continue.

------------------------------------------------------------------------

## Step 3.2 --- Define Output Structure

Add:

nlohmann::json output;

Nodes will write results here.

After completion print: Waiting to continue.

------------------------------------------------------------------------

# PHASE 4 --- Node Execution Interface

## Step 4.1 --- Create Virtual Execute Function

Modify Node to allow specialization.

virtual void execute();

Future nodes will override this.

After completion print: Waiting to continue.

------------------------------------------------------------------------

## Step 4.2 --- Add Node Status

Add status tracking.

enum class NodeStatus { IDLE, RUNNING, SUCCESS, FAILED };

Add field:

NodeStatus status;

After completion print: Waiting to continue.

------------------------------------------------------------------------

# PHASE 5 --- Node Registry

## Step 5.1 --- Create NodeRegistry

New files:

NodeRegistry.h NodeRegistry.cpp

Registry stores nodes by id.

Use:

std::unordered_map\<std::string, std::shared_ptr`<Node>`{=html}\>

After completion print: Waiting to continue.

------------------------------------------------------------------------

## Step 5.2 --- Register Nodes

Add function:

register_node()

Adds node to registry.

After completion print: Waiting to continue.

------------------------------------------------------------------------

## Step 5.3 --- Retrieve Nodes

Add function:

get_node(id)

Returns pointer to node.

After completion print: Waiting to continue.

------------------------------------------------------------------------

# PHASE 6 --- Node Graph (Preparation for Visual Layer)

## Step 6.1 --- Define NodeEdge

Create structure:

struct NodeEdge { std::string from; std::string to; };

After completion print: Waiting to continue.

------------------------------------------------------------------------

## Step 6.2 --- Create NodeGraph

Create class NodeGraph containing:

nodes edges

This will later power visual workflow execution.

After completion print: Waiting to continue.

------------------------------------------------------------------------

# PHASE 7 --- Node Execution Engine

## Step 7.1 --- Sequential Execution

Implement simple graph execution:

for each node execute()

After completion print: Waiting to continue.

------------------------------------------------------------------------

## Step 7.2 --- Pass Output to Next Node

Add simple rule:

next.input = previous.output

After completion print: Waiting to continue.

------------------------------------------------------------------------

# PHASE 8 --- Logging

## Step 8.1 --- Node Logging

Each node should log:

node start node finish node status

After completion print: Waiting to continue.

------------------------------------------------------------------------

# PHASE 9 --- Integration Hook

## Step 9.1 --- Add ExecutiveController Hook

Expose function:

execute_graph(NodeGraph graph)

ExecutiveController will call this.

After completion print: Waiting to continue.

------------------------------------------------------------------------

# FINAL RESULT

You will now have:

NODE runtime container Node types Execution engine Node registry Graph
structure

This becomes the backbone for:

-   tool nodes
-   LLM nodes
-   GRAG nodes
-   visual workflows
