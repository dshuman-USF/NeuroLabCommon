*       filename: miscellaneous_subroutines.f   (gathered 23-Jan-2004    lss)

*       This file contains miscellaneous subroutines useful for string manipulation:

*               remove_all_blanks
*               remove_leading_blanks
*               upper_case
*               lower_case
*               strlength  (added 14-Mar-2004)
*               concat (added 30-Nov-2004)
*               joinstrings (added 13-May-1005)


*       ******************************************************
c       Remove all spaces from string:  
*       ******************************************************
      
      subroutine remove_all_blanks(string,length)

      character*(*) string
      integer length

      if(.false.)print *,length !suppress unused variable warning

      do i = LEN(string),1,-1   !find the last non-blank character      
         if(string(i:i).ne.' ')exit
      end do
      ito=0
      do ifrom=1,i
         if(string(ifrom:ifrom).ne.' ')then !shift characters, if necessary, to remove the blanks
            ito=ito+1           ! from the interior of the string
            string(ito:ito)=string(ifrom:ifrom)
         end if
      end do
      string((ito+1):LEN(string))=' ' !fill the remainder of the string with blanks
      
      num_char = INDEX(string,' ')-1
      return
      end



*       *****************************************************************************************
c       Remove all invisible characters from string; only numbers and letters and spaces allowed:       
*       *****************************************************************************************
      
      subroutine validate_string(string,length,lastchar)

      character*(*) string
      integer length,lastchar

      j = 0
      do i = 1,length                              
         if(((ICHAR(string(i:i)).ge.48).and.
     +        (ICHAR(string(i:i)).le.57)).or.
     +        ((ICHAR(string(i:i)).ge.65).and.
     +        (ICHAR(string(i:i)).le.90)).or.
     +        ((ICHAR(string(i:i)).ge.97).and.
     +        (ICHAR(string(i:i)).le.122)).or.
     +        (string(i:i).eq.' '))then !good character - pass to the final string
            j = j+1
            string(j:j)=CHAR(ICHAR(string(i:i)))
         else                   !bad character - remove it from new string
         end if
      end do

      lastchar = j

      return
      end



*       ******************************************************
c       Remove leading spaces from string:      
*       ******************************************************
      
      subroutine remove_leading_blanks(string,length)

      character*(*) string
      integer length

      if(.false.)print *,length !suppress unused variable warning

      do i = 1,LEN(string)      !find the first non-blank character     
         if(string(i:i).ne.' ')exit
      end do

      do j = LEN(string),1,-1   !find the last non-blank character      
         if(string(j:j).ne.' ')exit
      end do

      string(1:j-i+1) = string(i:j) !move the text to the front of the string               
      string(j-i+2:LEN(string)) = ' ' !fill the rest of the string with blanks

      return
      end

*       ******************************************************
c       Remove leading spaces from string, return TRIM_LEN
*       ******************************************************
      
      subroutine left_justify(string,length)

      character*(*) string
      integer length

      do i = 1,LEN(string)      !find the first non-blank character     
         if(string(i:i).ne.' ')exit
      end do

      do j = LEN(string),1,-1   !find the last non-blank character      
         if(string(j:j).ne.' ')exit
      end do

      string(1:j-i+1) = string(i:j) !move the text to the front of the string               
      string(j-i+2:LEN(string)) = ' ' !fill the rest of the string with blanks
      length = j - i + 1
      return
      end


*       *****************************************************************
c       Force a text string to be ALL CAPS - only letters a-z are changed       
*       *****************************************************************

      subroutine upper_case(string,length)

      integer length
      character*(*) string

      if(.false.)print *,length !suppress unused variable warning

      
      do i = 1,LEN(string)      !force a text string to be all CAPS
         if((ICHAR(string(i:i)).ge.97).and.
     +        (ICHAR(string(i:i)).le.122))then
            string(i:i)=CHAR(ICHAR(string(i:i))-32)
         end if
      end do

      return
      end


*       ***********************************************************************
c       FORCE A TEXT STRING TO BE all lower case - only letters A-Z are changed 
*       ***********************************************************************

      subroutine lower_case(string,length)

      integer length
      character*(*) string

      if(.false.)print *,length !suppress unused variable warning
      
      do i = 1,LEN(string)      !force a text string to be all lower-case
         if((ICHAR(string(i:i)).ge.65).and.
     +        (ICHAR(string(i:i)).le.90))then
            string(i:i)=CHAR(ICHAR(string(i:i))+32)
         end if
      end do

      return
      end



*       **************************************************************
c       Find the location of the last (non-' ') character in a string:
*       **************************************************************

      subroutine strlength(string,length,lastchar)

      character*(*) string
      integer length, lastchar

      if(.false.)print *,length !suppress unused variable warning

      do i = LEN(string),1,-1   !find the last non-blank character      
         if(string(i:i).ne.' ')exit
      end do

      if(i.ne.0)then
         lastchar = i
      else
         lastchar = 1           !do not return a stringlength of 0
      end if

      return
      end

*       *************************************************************************
c       Join two text strings together and remove all spaces from the new string:
*       *************************************************************************


      subroutine concat(string1,string2,final_str,l)

      character*(*) string1,string2,final_str
      integer l,n,m

      call strlength(string1,LEN(string1),m)
      call strlength(string2,LEN(string2),n)
      if(m.eq.0)m=1
      if(n.eq.0)n=1
      final_str = string1(1:m)//string2(1:n)
      call remove_all_blanks(final_str,LEN(final_str))
      call strlength(final_str,LEN(final_str),l)
      return
      end

*       *************************************************************************
c       Join two text strings together, retaining all spaces in the new string:
*       *************************************************************************


      subroutine joinstrings(string1,string2,final_str,l)

      character*(*) string1,string2,final_str
      integer l,n,m

      call strlength(string1,LEN(string1),m)
      call strlength(string2,LEN(string2),n)
      if(m.eq.0)m=1
      if(n.eq.0)n=1
      final_str = string1(1:m)//string2(1:n)
      call strlength(final_str,LEN(final_str),l)
      return
      end
