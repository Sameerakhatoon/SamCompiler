# G02 - `double` primitive writes type into the size field

## Symptom

Parsing `double` produces a datatype whose `.type == DATA_TYPE_VOID`
(== 0) instead of `DATA_TYPE_DOUBLE`. Anything downstream that
switches on `.type` treats `double` as `void`. Not visible at the
parser stage but would break codegen / validator.

## Root cause

`parser.c::parser_datatype_init_type_and_size_for_primitive`, double
branch (from ch35):

```c
} else if(S_EQ(datatype_token->sval, "double")){
    datatype_out->size = DATA_TYPE_DOUBLE;   // <-- wrong field
    datatype_out->size = DATA_SIZE_DWORD;
}
```

The first line should set `.type`, not `.size`. The next line
overwrites `.size`, so the only visible damage is `.type` staying 0.
PeachCompiler ships it this way verbatim.

## Fix

```c
} else if(S_EQ(datatype_token->sval, "double")){
    datatype_out->type = DATA_TYPE_DOUBLE;
    datatype_out->size = DATA_SIZE_DWORD;
}
```

## Lesson

Big copy-pasted switch arms are prone to this. A table-driven
approach (struct of {name, type, size}) would have moved the
field-naming into one place and ruled this class out.
