#!/usr/bin/env bats

setup() {
    MSH="$BATS_TEST_DIRNAME/../msh"
}

@test "basic command execution" {
    run bash -c "echo 'echo hello' | $MSH"
    [ "$status" -eq 0 ]
    [[ "$output" == *"hello"* ]]
}

@test "cd builtin changes directory" {
    run bash -c "cd /tmp && echo 'pwd' | $MSH"
    [[ "$output" == *"/tmp"* ]]
}

@test "redirection writes to file" {
    run bash -c "echo 'echo hi > /tmp/msh_test.txt' | $MSH && cat /tmp/msh_test.txt"
    [[ "$output" == *"hi"* ]]
}

@test "append redirection" {
    run bash -c "printf 'echo one > /tmp/msh_test2.txt\necho two >> /tmp/msh_test2.txt\n' | $MSH && cat /tmp/msh_test2.txt"
    [[ "$output" == *"one"* ]]
    [[ "$output" == *"two"* ]]
}

@test "pipe passes output between commands" {
    run bash -c "echo 'echo hello | wc -l' | $MSH"
    [[ "$output" == *"1"* ]]
}

@test "&& runs second command only on success" {
    run bash -c "echo 'true && echo yes' | $MSH"
    [[ "$output" == *"yes"* ]]
}

@test "&& skips second command on failure" {
    run bash -c "echo 'false && echo should_not_print' | $MSH"
    [[ "$output" != *"should_not_print"* ]]
}

@test "|| runs second command only on failure" {
    run bash -c "echo 'false || echo yes' | $MSH"
    [[ "$output" == *"yes"* ]]
}

@test "pipeline exit status reflects last command" {
    run bash -c "echo 'false | true && echo pipeline_ok' | $MSH"
    [[ "$output" == *"pipeline_ok"* ]]
}

@test "exit builtin terminates shell" {
    run bash -c "echo 'exit' | gtimeout 2 $MSH"
    [ "$status" -eq 0 ]
}