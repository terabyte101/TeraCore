@echo OFF
@title Extract...
@color 03
mapextractor.exe
vmap4extractor.exe 
md vmaps 
vmap4assembler.exe Buildings vmaps  
pause