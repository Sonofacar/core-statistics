# Core Statistical Modeling

Collection of tools making it possible to wrangle and model data from the
command line.

**Work in progress**

These tools are written in a collection of C and shell script with the
intention to simplify the process of data cleaning and modeling from the
command line. They rely on the GNU Scientific Library being present on the
system this is to be installed on for purposes of modeling.

To-do list:
- [ ] Parse a CSV from stdin
    - [X] Read column names
    - [X] Interpret numeric values
    - [ ] Interpret character values (as categories)
- [ ] lm (linear model)
    - [X] Create a useful linear model
    - [ ] Create command line options
        - [ ] Dynamic output
            - [ ] Decide how to output:
                - residuals/stderr
                - coefficients
                - model diagnostics
                - predictions
        - [ ] train/test sets?
        - [ ] transformation of response variable
        - encode categories
		- [X] dummy encoding
		- [ ] target encoding
    - [ ] Dynamically handle long matrices
- [ ] plm (penalized linear regression)
