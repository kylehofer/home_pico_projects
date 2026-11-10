#ifndef DEPENDENCY_EXECUTOR
#define DEPENDENCY_EXECUTOR

#include <functional>
#include <vector>
#include <map>

typedef int DependencyToken;
typedef std::function<bool()> DependencyFunction;

struct DependencyEntry
{
    DependencyFunction function;
    DependencyToken dependent;
};

/**
 * @brief A class for creating a graph of functions so that
 * a functions execution can depend on the success of another
 *
 */
class DependencyExecutor
{
private:
    std::map<DependencyToken, DependencyEntry> executors;
    DependencyToken next = 0;

protected:
public:
    DependencyExecutor();
    ~DependencyExecutor();

    /**
     * @brief Adds a function to the dependency executor
     * This function will not depend on any other and will always execute
     *
     * @param function
     * @return DependencyToken The token of this newly added function
     */
    DependencyToken add(DependencyFunction function);
    /**
     * @brief  Adds a function to the dependency executor
     * This function will depend on the token of another function
     *
     * @param dependant The token of the function that this function will depend on
     * @param function
     * @return DependencyToken The token of this newly added function
     */
    DependencyToken add(DependencyToken dependant, DependencyFunction function);

    /**
     * @brief Adds a chain of functions that will depend on each other
     *
     * @param functions
     * @return DependencyToken
     */
    DependencyToken addChain(const std::vector<DependencyFunction> &functions);
    /**
     * @brief Adds a chain of functions that will depend on each other
     *
     * @param dependant The dependant that this chain will depend on
     * @param functions
     * @return DependencyToken
     */
    DependencyToken addChain(DependencyToken dependant, const std::vector<DependencyFunction> &functions);

    /**
     * @brief Executes the entire dependency chain of functions
     *
     * @return true If all functions executed successfully
     * @return false
     */
    bool execute();
};

#endif /* DEPENDENCY_EXECUTOR */
