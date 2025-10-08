# Core Statistical Modeling

Collection of tools making it possible to wrangle and model data from the
command line.

**Work in progress**

These tools are written in a collection of C and shell script with the
intention to simplify the process of data cleaning and modeling from the
command line. They rely on the GNU Scientific Library being present on the
system this is to be installed on for purposes of modeling.

To-do list:
- [X] Parse a CSV from stdin
	- [X] Read column names
	- [X] Interpret numeric values
	- [X] Interpret character values (as categories)
		- encode categories
			- [X] dummy encoding
			- [ ] target encoding
- [ ] lm (linear model)
	- [X] Create a useful linear model
	- [ ] Create command line options
		- [o] Dynamic output
			- giving name outputs model files
			- no name prints summary to stdout
		- [ ] train/test sets?
		- [X] transformation of response variable
		- [X] category encoding type
	- [ ] Dynamically handle long matrices
- [ ] plm (penalized linear model)
