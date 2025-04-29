For if boolean statements in C++, do I always have to indicate true or false processes, or can I just us a singular if (true or false parameter) and use a return to stop the process if true, and continue if false?

Yes, absolutely! You do not need an else block for every if statement.
You can definitely use a singular if statement to check a condition and use return within that if block to exit the function (stop the process) if the condition is true. The code after the if statement will then only execute if the condition was false (because the return would have been skipped).
This is a very common and often preferred pattern, sometimes called a "guard clause" or "early exit". It helps reduce nesting and can make the main logic of the function clearer.
Here's how it works:
Scenario 1: Using if / else
#include <iostream>

bool processData(int data) {
    bool isValid = (data > 0); // Example validity check

    if (isValid) {
        // --- True Process ---
        std::cout << "Processing valid data: " << data << std::endl;
        // ... do more work with valid data ...
        return true; // Indicate success
    } else {
        // --- False Process ---
        std::cout << "Invalid data encountered: " << data << std::endl;
        return false; // Indicate failure or invalid state
    }
    // Code here is unreachable in this specific example because both paths return
}
Use code with caution.
C++
Scenario 2: Using Singular if with return (Early Exit / Guard Clause)
This achieves the same logical outcome but is structured differently.
#include <iostream>

// Option A: Stop if a "stop condition" is TRUE (Matches your description)
bool processDataA(int data) {
    bool shouldStop = (data <= 0); // Check if we should stop early

    if (shouldStop) { // If the stop condition is true...
        std::cout << "Stopping early due to invalid data: " << data << std::endl;
        return false; // ...return immediately (stop the process for this function)
    }

    // --- Code here only runs if 'shouldStop' was FALSE ---
    // This effectively becomes the "continue if false" part
    std::cout << "Continuing processing for valid data: " << data << std::endl;
    // ... do more work with valid data ...
    return true; // Indicate success
}

// Option B: More common "Guard Clause" - Check for validity and return if *invalid*
bool processDataB(int data) {
    if (data <= 0) { // Check if data is INVALID
        std::cout << "Invalid data encountered: " << data << std::endl;
        return false; // Return early if invalid
    }

    // --- Code here only runs if the 'if' condition was FALSE (i.e., data was VALID) ---
    std::cout << "Processing valid data: " << data << std::endl;
    // ... do more work with valid data ...
    return true; // Indicate success
}
Use code with caution.
C++
Key Points:
else is Optional: An if statement only requires the block that executes when the condition is true.
return Exits: The return statement immediately exits the current function.
Readability: Using early exits (if (condition) { return; }) at the beginning of a function to handle errors, preconditions, or simple cases often makes the main "success path" of the function less indented and easier to follow.
Return Value: Remember that the return statement must match the function's declared return type (e.g., return; for void functions, return false; for bool functions, return nullptr; for pointer functions, return 0; for int functions etc.).
So yes, your proposed approach of using if (condition) { return; } is perfectly valid C++ and a common practice.