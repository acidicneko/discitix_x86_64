-- Simple Calculator in Lua

local function calculate()
    while true do
        print("\n--- Simple Lua Calculator ---")
        
        -- Get the first number
        io.write("Enter first number (or 'q' to quit): ")
        local input1 = io.read()
        if input1 == 'q' or input1 == 'Q' then break end
        local num1 = tonumber(input1)
        
        -- Get the operator
        io.write("Enter operator (+, -, *, /): ")
        local op = io.read()
        
        -- Get the second number
        io.write("Enter second number: ")
        local input2 = io.read()
        local num2 = tonumber(input2)
        
        -- Validate numbers
        if not num1 or not num2 then
            print("Error: Invalid numbers entered. Please try again.")
        else
            -- Perform calculation based on operator
            if op == "+" then
                print("Result: " .. (num1 + num2))
            elseif op == "-" then
                print("Result: " .. (num1 - fb2))
            elseif op == "*" then
                print("Result: " .. (num1 * num2))
            elseif op == "/" then
                if num2 == 0 then
                    print("Error: Division by zero is not allowed.")
                else
                    print("Result: " .. (num1 / num2))
                end
            else
                print("Error: Invalid operator '" .. op .. "'")
            end
        end
    end
    print("Goodbye!")
end

-- Run the calculator
calculate()
