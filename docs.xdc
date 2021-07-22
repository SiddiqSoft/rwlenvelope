<?xml version="1.0"?><doc>
<members>
<member name="T:nlohmann.detail.position_t" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="85">
struct to capture the start position of the current token
</member>
<member name="F:nlohmann.detail.position_t.chars_read_total" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="88">
the total number of characters read
</member>
<member name="F:nlohmann.detail.position_t.chars_read_current_line" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="90">
the number of characters read in the current line
</member>
<member name="F:nlohmann.detail.position_t.lines_read" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="92">
the number of lines read
</member>
<member name="M:nlohmann.detail.position_t.op_Implicit~System.UInt64" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="95">
conversion to size_t to preserve SAX interface
</member>
<member name="M:nlohmann.detail.exception.what" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="2355">
returns the explanatory string
</member>
<member name="F:nlohmann.detail.exception.id" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="2362">
the id of the exception
</member>
<member name="F:nlohmann.detail.exception.m" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="2375">
an exception object as storage for error messages
</member>
<member name="T:nlohmann.detail.input_format_t" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="4743">
the supported input formats
</member>
<member name="F:nlohmann.detail.file_input_adapter.m_file" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="4776">
the file pointer to read from
</member>
<member name="F:nlohmann.detail.input_stream_adapter.is" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="4835">
the associated input stream
</member>
<member name="T:nlohmann.detail.cbor_tag_handler_t" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="7667">
how to treat CBOR tags
</member>
<member name="F:object_start" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="10139">
the parser read `{` and started to process a JSON object
</member>
<member name="F:object_end" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="10141">
the parser read `}` and finished processing a JSON object
</member>
<member name="F:array_start" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="10143">
the parser read `[` and started to process a JSON array
</member>
<member name="F:array_end" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="10145">
the parser read `]` and finished processing a JSON array
</member>
<member name="F:key" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="10147">
the parser read a key of a value in an object
</member>
<member name="F:value" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="10149">
the parser finished reading a JSON value
</member>
<member name="F:nlohmann.detail.primitive_iterator_t.m_it" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="10643">
iterator as signed integer type
</member>
<member name="M:nlohmann.detail.primitive_iterator_t.set_begin" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="10652">
set iterator to a defined beginning
</member>
<member name="M:nlohmann.detail.primitive_iterator_t.set_end" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="10658">
set iterator to a defined past the end
</member>
<member name="M:nlohmann.detail.primitive_iterator_t.is_begin" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="10664">
return whether the iterator can be dereferenced
</member>
<member name="M:nlohmann.detail.primitive_iterator_t.is_end" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="10670">
return whether the iterator is at end
</member>
<member name="T:nlohmann.detail.error_handler_t" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="15468">
how to treat decoding errors
</member>
<member name="T:std.hash{nlohmann.basic_json&lt;std.map,std.vector,std.basic_string&lt;System.SByte!System.Runtime.CompilerServices.IsSignUnspecifiedByte,std.char_traits{System.SByte!System.Runtime.CompilerServices.IsSignUnspecifiedByte},std.allocator&lt;System.SByte!System.Runtime.CompilerServices.IsSignUnspecifiedByte&gt;&gt;,System.Boolean,System.Int64,System.UInt64,System.Double,std.allocator,nlohmann.adl_serializer,std.vector&lt;System.Byte,std.allocator&lt;System.Byte&gt;&gt;&gt;}" decl="false" source="C:\Users\maas\source\repos\siddiqsoft\RWLEnvelope\packages\nlohmann.json.3.9.1\build\native\include\nlohmann\json.hpp" line="25186">
hash value for JSON objects
</member>
<!-- Discarding badly formed XML document comment for member 'T:std.less{<unknown type>}'. -->
</members>
</doc>