package mdhim;
import org.bridj.BridJ;
import org.bridj.Pointer;
import org.bridj.StructObject;
import org.bridj.ann.Field;
import org.bridj.ann.Library;
/**
 * <i>native declaration : mdhim-tng-ycsb/src/range_server.h:1360</i><br>
 * This file was autogenerated by <a href="http://jnaerator.googlecode.com/">JNAerator</a>,<br>
 * a tool written by <a href="http://ochafik.com/">Olivier Chafik</a> that <a href="http://code.google.com/p/jnaerator/wiki/CreditsAndLicense">uses a few opensource projects.</a>.<br>
 * For help, please visit <a href="http://nativelibs4java.googlecode.com/">NativeLibs4Java</a> or <a href="http://bridj.googlecode.com/">BridJ</a> .
 */
@Library("mdhim") 
public class work_queue extends StructObject {
	static {
		BridJ.register();
	}
	/** C type : work_item* */
	@Field(0) 
	public Pointer<work_item > head() {
		return this.io.getPointerField(this, 0);
	}
	/** C type : work_item* */
	@Field(0) 
	public work_queue head(Pointer<work_item > head) {
		this.io.setPointerField(this, 0, head);
		return this;
	}
	/** C type : work_item* */
	@Field(1) 
	public Pointer<work_item > tail() {
		return this.io.getPointerField(this, 1);
	}
	/** C type : work_item* */
	@Field(1) 
	public work_queue tail(Pointer<work_item > tail) {
		this.io.setPointerField(this, 1, tail);
		return this;
	}
	public work_queue() {
		super();
	}
	public work_queue(Pointer pointer) {
		super(pointer);
	}
}