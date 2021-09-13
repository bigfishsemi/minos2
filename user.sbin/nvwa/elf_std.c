/*
 * Copyright (C) 2021 Min Le (lemin9538@gmail.com)
 * Copyright (c) 2021 上海网返科技
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <minos/types.h>
#include <minos/debug.h>

#include "elf.h"

static int elf_file_read(FILE *file, void *buf, size_t size, off_t offset)
{
	int ret;

	ret = fseek(file, offset, SEEK_SET);
	if (ret < 0)
		return ret;

	if (fread(buf, 1, size, file) != size) {
		pr_err("read elf file 0x%lx\n fail\n", offset);
		return -EIO;
	}

	return 0;
}

static int elf_findphdr(FILE *file, struct elf_ctx *ctx,
		Elf_Phdr *phdr, uint32_t type, unsigned *i)
{
	int rv = EL_OK;

	for (; *i < ctx->ehdr.e_phnum; (*i)++) {
		rv = elf_file_read(file, phdr, sizeof(Elf_Phdr), EL_PHOFF(ctx, *i));
		if (rv)
			return rv;

		if (phdr->p_type == type)
			return rv;
	}

	*i = -1;
	return rv;
}

int elf_init(struct elf_ctx *ctx, FILE *file)
{
	Elf_Phdr ph;
	int rv = EL_OK;
	unsigned i = 0;

	memset(ctx, 0, sizeof(struct elf_ctx));

	if ((rv = elf_file_read(file, &ctx->ehdr, sizeof(ctx->ehdr), 0)))
		return rv;

	if (!IS_ELF(ctx->ehdr))
		return EL_NOTELF;

	if (ctx->ehdr.e_ident[EI_CLASS] != ELFCLASS)
	        return EL_WRONGBITS;

	if (ctx->ehdr.e_ident[EI_DATA] != ELFDATATHIS)
	        return EL_WRONGENDIAN;

	if (ctx->ehdr.e_ident[EI_VERSION] != EV_CURRENT)
	        return EL_NOTELF;

	if (ctx->ehdr.e_type != ET_EXEC || ctx->ehdr.e_type == ET_DYN)
	        return EL_NOTEXEC;

	if (ctx->ehdr.e_machine != EM_THIS)
	        return EL_WRONGARCH;

	if (ctx->ehdr.e_version != EV_CURRENT)
	        return EL_NOTELF;

	/*
	 * calculate how many memory is needed for this elf file, the
	 * memory will allocated together.
	 */
	ctx->base_load_vbase = (unsigned long)-1;

	for(;;) {
		if ((rv = elf_findphdr(file, ctx, &ph, PT_LOAD, &i)))
			return rv;

	        if (i == (unsigned) -1)
			break;

		if (ph.p_vaddr < ctx->base_load_vbase)
			ctx->base_load_vbase = ph.p_vaddr;

	        Elf_Addr phend = ph.p_vaddr + ph.p_memsz;
	        if (phend > ctx->base_load_vend)
			ctx->base_load_vend = phend;

	        if (ph.p_align > ctx->align)
			ctx->align = ph.p_align;
		i++;
	}

	ctx->memsz = PAGE_BALIGN(ctx->base_load_vend - ctx->base_load_vbase);

	return rv;
}

static int elf_load_section(FILE *file, void *vaddr, Elf_Shdr *shdr)
{
	/*
	 * bss section ?
	 */
	if (shdr->sh_type == SHT_NOBITS) {
		pr_info("bzero elf section [0x%lx 0x%lx 0x%lx %d]\n",
			shdr->sh_offset, shdr->sh_addr, shdr->sh_size, shdr->sh_type);
		memset(vaddr, 0, shdr->sh_size);
		return 0;
	}

	pr_info("loading elf section [0x%lx 0x%lx 0x%lx %d]\n",
			shdr->sh_offset, shdr->sh_addr, shdr->sh_size, shdr->sh_type);
	elf_file_read(file, vaddr, shdr->sh_size, shdr->sh_offset);

	return 0;
}

static int elf_findshdr(FILE *file, struct elf_ctx *ctx,
		Elf_Shdr *shdr, unsigned int *i)
{

	int rv = EL_OK;
	int j = *i;

	for ( ;j < ctx->ehdr.e_shnum; j++) {
		rv = elf_file_read(file, shdr, sizeof(Elf64_Ehdr), EL_SHOFF(ctx, j));
		if (rv)
			return rv;

		if (shdr->sh_flags & SHF_ALLOC) {
			*i = j;
			return 0;
		}
	}

	*i = -1;
	return rv;
}

int load_process_from_file(void *page, struct elf_ctx *ctx, FILE *file)
{
	int rv = EL_OK;
	unsigned int i = 0;
	void *vaddr;
	Elf_Shdr shdr;

	for(;;) {
		if ((rv = elf_findshdr(file, ctx, &shdr, &i)))
			return rv;

		if (i == (unsigned int)-1)
			break;

		vaddr = page + (shdr.sh_addr - ctx->base_load_vbase);
		rv = elf_load_section(file, vaddr, &shdr);
		if (rv)
			return EL_ENOMEM;
		i++;
	}

	return rv;
}
