# SDK Layout and Overview

## Basic Functionality

The following pages document the core of the API, every application will use these portions of the SDK:

* [client.h](@ref client.h) Client initialization and control
* [config.h](@ref config.h) Global client configuration
* [user.h](@ref user.h) Configuration of individual user objects
* [variations.h](@ref variations.h) Evaluation related functions such as `LDClientBoolVariation`

## Extra Features

* [json.h](@ref json.h) JSON manipulation functions. Required for use of JSON flags.
* [logging.h](@ref logging.h) Allows specification of custom log handlers
* [memory.h](@ref memory.h) Allows specification of custom memory allocators
* [flag_state.h](@ref flag_state.h) Functionality related to `LDAllFlagsState`.
