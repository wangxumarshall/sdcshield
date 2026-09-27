#!/bin/bash

# This script checks the sanity of the history on the branch:
#   * there are no merge commits on the branch;
#   * all the commits have sign-off-by as required.

# When the PR branch is checked out in CI actions, it's a temporary merge
# commit. This script looks at the right parent to see if all the non-merge
# commit follow the requirements.

target=$1
head=$2
err=0


read left right <<<$(git show -s --pretty=%P ${head})

if [[ -z "${right}" ]]; then
    echo "::error:: ${head} is expected to merge commit for the PR. (Cannot retrieve tip of the branch.)"
fi

base=$(git merge-base origin/${target} ${right})

# check there are no merges on the branch
merges=$(git log --oneline --merges ${base}..${right})

if [[ -n "${merges}" ]]; then
    echo "::error:: The branch contains merge commits. Rebase your work on top of current ${target}."
    err=1
fi

# Copilot Autofix commits (code scanning "Security and quality" alerts, pushed
# by GitHub on alert-autofix-* branches) are created by the GHAS bot and cannot
# carry a human Signed-off-by at creation time; the human certifies them by
# merging the PR. Identified by BOTH markers: committed by GitHub on behalf of
# the bot AND carrying the bot's co-authored-by trailer. Both markers are
# unauthenticated commit metadata: they raise the bar (a local commit with a
# crafted trailer alone does not pass), but a determined contributor can forge
# both. The maintainer's merge review remains the actual certification, exactly
# as with any Signed-off-by line.
commit_is_copilot_autofix() {
    local sha=$1
    local committer trailer
    committer=$(git show -s --format='%cn <%ce>' ${sha})
    trailer=$(git show -s --format=%B ${sha} | grep -F 'github-advanced-security[bot]@users.noreply.github.com')
    [[ "${committer}" == "GitHub <noreply@github.com>" && -n "${trailer}" ]]
}

# look sign offs in all the non-merge commits
for sha in $(git log --no-merges --format=%H ${base}..${right}); do
    signoff=$(git show -s --format=%B ${sha} | grep '^Signed-off-by:')
    if [[ -z "${signoff}" ]]; then
        if commit_is_copilot_autofix ${sha}; then
            echo "::notice:: Commit ${sha} has no Signed-off-by but is a Copilot Autofix commit (github-advanced-security[bot]) - exempt."
        else
            echo "::error:: Commit ${sha} does not contain Signed-off-by. Rebase and amend with 'git commit --amend --signoff'."
            err=1
        fi
    fi
done
exit $err
